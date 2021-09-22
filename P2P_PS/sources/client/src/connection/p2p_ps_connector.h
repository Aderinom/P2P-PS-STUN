#pragma once
#include "connection_i.h"
#include "udp_socket.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <vector>

#include <common/log.h>
#include <common/messages.h>
#include <common/time_helper.h>






namespace network
{

	constexpr uint32_t scan_range = 50;

    struct p2p_ps_connector_t : 
		public connector_i
	{
	public:	

		enum state  
		{
			invalid,
			disconnected,
			waiting_for_stun_reply,
			waiting_for_client_reply,
			connected,
		};
		bool is_sym_nat = true;
		
		address_t stun_server_address1;
		address_t stun_server_address2;
		uint32_t transaction_id_ = 0;
		uint16_t base_port = 0;
		
	private:
		state state = disconnected;

	public:
	
		connect_result_e connect( socket_i & in_socket )
		{	
			
			udp_socket_t & socket = reinterpret_cast<udp_socket_t&>(in_socket);
			udp_socket_t socket2;

			sockaddr_in stun_server_addr1;
			sockaddr_in stun_server_addr2;
			bool connection_established = false;
			int result;
			message_t message;

			log_t::debug("Starting Connection to %hhu.%hhu.%hhu.%hhu:%hu//%hhu.%hhu.%hhu.%hhu:%hu",
				stun_server_address1.ip_parts[3], stun_server_address1.ip_parts[2],
				stun_server_address1.ip_parts[1], stun_server_address1.ip_parts[0],
				stun_server_address1.port, 
				stun_server_address2.ip_parts[3], stun_server_address2.ip_parts[2],
				stun_server_address2.ip_parts[1], stun_server_address2.ip_parts[0],
				stun_server_address2.port
			);

			//
			// Send First Request to server
			//

			memset(&stun_server_addr1,0, sizeof(sockaddr_in));
		
			stun_server_addr1.sin_addr.s_addr = htonl(stun_server_address1.ip);
			stun_server_addr1.sin_port = htons(stun_server_address1.port);
			stun_server_addr1.sin_family = AF_INET;

			base_port = socket.bind_by_portman(0);
			if(!base_port)
			{
				log_t::warning("Couldn't bind base socket1 to port recommended by portman | %s",strerror(errno));
			}

			socket.set_target_address(stun_server_addr1);

			memset(&message,0, sizeof(message_t));
			message.type = message.ps_bind_request;
			message.message.bind_request.transaction_id = htonl(transaction_id_);

			if (!socket.send((const uint8_t*)&message,sizeof(message))) 
			{
				log_t::error("Failed when Sending stun request | %s", strerror(errno));
				return connect_result_e::connection_error;
			}

			result = socket.wait_receive((uint8_t*)&message, sizeof(message_t),5);
			if(result == -1 )
			{
				log_t::error("Error on receiving during connection establishment | %s", strerror(errno));
				return connect_result_e::connection_error;
			}
			else if (result == 0)
			{
				log_t::error("Server did not ack packet in time | %s", strerror(errno));
				return connect_result_e::connection_error;
			}

			//
			// Send Second Request to Server
			//
			{
				udp_socket_t & helper_socket = is_sym_nat ? socket2 : socket;

				if(is_sym_nat)
				{
					if(!helper_socket.init())
					{	
						log_t::error("Failed initializing helper socket | %s", strerror(errno));
						return connect_result_e::connection_error;
					}
				}
				

				sleep(1);

				if(is_sym_nat)
				{
					if(!helper_socket.bind_by_portman(base_port))
					{
						log_t::warning("Couldn't bind base socket2 to port recommended by portman | %s",strerror(errno));
					}
				}

				memset(&stun_server_addr2,0, sizeof(sockaddr_in));
			
				stun_server_addr2.sin_addr.s_addr = htonl(stun_server_address2.ip);
				stun_server_addr2.sin_port = htons(stun_server_address2.port);
				stun_server_addr2.sin_family = AF_INET;

				helper_socket.set_target_address(stun_server_addr2);
				
				message.type = message.ps_bind_request;
				message.message.bind_request.transaction_id = htonl(transaction_id_);

				if (!helper_socket.send((const uint8_t*)&message,sizeof(message))) 
				{
					log_t::error("Failed when Sending stun request | %s", strerror(errno));
					return connect_result_e::connection_error;
				}

				result = helper_socket.wait_receive((uint8_t*)&message, sizeof(message_t),5);
				if(result == -1 )
				{
					log_t::error("Error on receiving during connection establishment | %s", strerror(errno));
					return connect_result_e::connection_error;
				}
				else if (result == 0)
				{
					log_t::error("Server did not ack packet in time | %s", strerror(errno));
					return connect_result_e::connection_error;
				}
			}
			
			//
			// Send Second Request to Server
			//

			state = state::waiting_for_stun_reply;

			uint32_t attempts_left = 5;
			sockaddr_in peer_addr;

			while (!connection_established && attempts_left)
			{
				
				//
				// Await and read reqsponse from server
				//

				result = socket.wait_receive((uint8_t*)&message, sizeof(message_t),1);
				peer_addr = socket.get_peer_address();
				if (result == 0) {

					switch (state)
					{
						case state::waiting_for_stun_reply :
							log_t::debug("Still waiting for STUN Server Reply");
							break;
						case state::waiting_for_client_reply :
							log_t::debug("Still waiting for Peer Client Reply");
							break;

						default:
							break;
					}
					
					attempts_left--;
					continue;
				}
				else if (result < 0) {
					log_t::debug("Error while receiving socket  | %s", strerror(errno));
					attempts_left--;
					continue;
				}
				
				switch (message.type)
				{
				case message.ps_bind_reply:
					{
						if (peer_addr.sin_addr.s_addr != stun_server_addr2.sin_addr.s_addr &&
							peer_addr.sin_addr.s_addr != stun_server_addr1.sin_addr.s_addr)
						{
							log_t::warning("Got bind reply from unknown host %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
							continue;
						}
						
						int success = try_start_ps_connection(message.message.ps_bind_reply, socket);

						if(success == -1)
						{
							log_t::warning("Received ports did not give predictable results");
							return connect_result_e::connection_failed;
						}
						else if (success == 0)
						{
							log_t::warning("Port Prediction failed");
							return connect_result_e::connection_failed;
						}
						attempts_left = 2;
					}
					break;

				case message.client_bind_request:

					if (reply_to_bind_request(socket,message.message.client_bind_request, peer_addr)) {
						log_t::success("Successfully connected to peer %s:%d", inet_ntoa(socket.get_target_address().sin_addr), ntohs(socket.get_target_address().sin_port));
						connection_established = true;
						state = state::connected;
						continue;
					};

					break;
				case message.client_bind_response:

					if (peer_addr.sin_addr.s_addr != socket.get_target_address().sin_addr.s_addr ||
						peer_addr.sin_port != socket.get_target_address().sin_port)
					{
						log_t::warning("Got client_bind_response from unknown host %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
						continue;
					}
					state = state::connected;
					log_t::success("Successfully connected to peer  %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
					connection_established = true;
					continue;
					break;

				case message.ack:
					log_t::debug("Got Ack from server  %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
					//sleep(1);
					//attempts_left--;
					continue;
					break;

				default:
					log_t::warning("Received unexpected packet - %hhu", message.type);
					break;
				}
		
				attempts_left--;
			}

			switch (state)
			{
			case state::connected:
					return connect_result_e::connection_successful; 
				break;
			case state::waiting_for_stun_reply:
					log_t::log("Failed p2p Connection because server did not answer");
					return connect_result_e::connection_failed_no_server_match_in_time;
				break;
			default:
					log_t::log("Failed p2p Connection");
					return connect_result_e::connection_failed;
				break;
			}
		}

		// -1 not possible
		// 0 failed
		// 1 success 
		int try_start_ps_connection(message_t::ps_bind_reply_t & bind_reply, udp_socket_t & in_socket )
		{
			uint16_t p1 = ntohs(bind_reply.addr_one.sin_port); 
			uint16_t p2 = ntohs(bind_reply.addr_two.sin_port); 
			uint16_t target_port = determine_target_port(p1,p2, bind_reply.T1, bind_reply.T2, scan_range);
			if(target_port == 0) return -1;

			sockaddr_in addr = bind_reply.addr_one;
			timeval timeout = { 3, 0 };
			fd_set rfds;
			FD_ZERO(&rfds);
			addr.sin_port = htons(target_port);

			

			if(is_sym_nat)
			{
				if(target_port == p1)
				{
					log_t::statistic("[SYM]BASE %hu -> %hu TRG[NRM] (%hu::%hu dT %llu)", base_port, target_port, p1, p2,bind_reply.T2 - bind_reply.T1);
				}else{
					log_t::statistic("[SYM]BASE %hu -> %hu TRG[SYM] (%hu::%hu dT %llu)", base_port, target_port, p1, p2,bind_reply.T2 - bind_reply.T1);
				}

				// Symmetric NAT
				std::vector<udp_socket_t> sockets(scan_range);
				bool success = false;
				int max_sock_addr = 0;
	

				for (uint32_t i = 0; !success && i < scan_range; i++)
				{	
					//sockets[i].reset(new udp_socket_t);
					udp_socket_t & socket = sockets[i] = udp_socket_t();
			
					if(!socket.init()) return -1;
					
					if(!socket.bind_by_portman(base_port))
					{
						log_t::warning("Couldn't bind scan socket to port recommended by portman | %s",strerror(errno));
					}
					socket.set_target_address(addr);

					int socket_handle = socket.get_socket();
					if(max_sock_addr < socket_handle) max_sock_addr = socket_handle;
					FD_SET(socket_handle, &rfds);
					
					try_connect_to_client(socket);
				}	

				int result = select(max_sock_addr + 1, &rfds, NULL, NULL, &timeout);
				
				if(result == 0)
				{
					log_t::log("P2P Port predicted Connection Failed");
					return 0;
				} 
				else if(result == -1)
				{
					log_t::error("Error on receiving during select on sockets | %s", strerror(errno));
					return 0;
				}


				for (uint32_t i = 0; i < scan_range; i++)
				{
					
					if(!FD_ISSET(sockets[i].get_socket(), &rfds)) continue;
					std::swap(in_socket, sockets[i]);
					
					return 1;
				}		

			}
			else
			{
				if(target_port == p1)
				{
					log_t::statistic("[NRM]BASE %hu -> %hu TRG[NRM] (%hu::%hu dT %llu)", base_port, target_port, p1, p2,bind_reply.T2 - bind_reply.T1);
				}else{
					log_t::statistic("[NRM]BASE %hu -> %hu TRG[SYM] (%hu::%hu dT %llu)", base_port, target_port, p1, p2,bind_reply.T2 - bind_reply.T1);
				}

				in_socket.set_target_address(addr);
				
				try_connect_to_client(in_socket);

				int socket_handle = in_socket.get_socket();
				FD_SET(socket_handle, &rfds);
				int result = select(socket_handle + 1, &rfds, NULL, NULL, &timeout);

				if(result == 0)
				{
					log_t::log("P2P Port predicted Connection Failed");
					return 0;
				} 
				else if(result == -1)
				{
					log_t::error("Error on receiving during select on sockets | %s", strerror(errno));
					return 0;
				}
				return 1;
			}	

			return 0;
		}


		uint16_t determine_target_port(uint16_t first_port, uint16_t second_port, uint64_t T1, uint64_t T2, uint32_t scan_range)
		{

			if(first_port == second_port) return first_port;

			log_t::debug("Determining ports %hu:%hu with Tdiff %llu",first_port, second_port ,T2 - T1);
			if(first_port + 250 > second_port && first_port - 250 < second_port)
			{		
				// Is Probably sequential
				uint64_t time_now = get_time_in_ms();
				uint16_t distance = second_port - first_port;
				uint64_t timediff_port_allocation = T2 - T1 + 1;
				uint64_t timediff_now = time_now - T2;

				float parts = (float)(timediff_now + 100) / timediff_port_allocation; //Current time difference plus roundtrip estimate untill packet reaches partner divided by timediff between allocations;
				
				if(distance > 0)
				{
					return second_port + (parts * distance) + (scan_range)/2;
				}
				else
				{
					return second_port + (parts * distance) - (scan_range)/2;
				}
			}
			else
			{
				// Is Probably random
				// There might be a opportunity here to research pseudo random number generators.
				// this would differ between vendor of the Nat box, but there is a good chance that many vendors use the same kind of PRNG,
				// Without a system to predict the next random number the probability would be to low to even try
				// 1/64000 -> 0,0016% chance for each prediction. so to get even a 50% chance we would have to predict and test over 62000 ports.
				// If we were also a random nat the chance is even smaller since the partner would have to guess correctly as well.
				return 0;
			}

			// there is also a chance that by the time the second port was reached the Port allocation limit was hit and it starts back at the beginning. This case is not handled. 
		};


		bool try_connect_to_client(udp_socket_t & socket)
		{
			message_t message;
			message.type = message.client_bind_request;
			message.message.client_bind_request.transaction_id = htonl(transaction_id_);
			auto target = socket.get_target_address();
			
			log_t::debug("Trying to connect to client %s:%hu ", inet_ntoa(target.sin_addr), ntohs(target.sin_port));
			
			int result = socket.send((uint8_t *)&message, sizeof(message_t));
			if (result == -1) {
				log_t::error("Failed sending client_bind_respone | %s", strerror(errno));
				return false;
			}

			return true;
		}

		bool reply_to_bind_request(socket_i & socket, const message_t::client_bind_request_t & client_bind_request, sockaddr_in & peer_addr)
		{
			log_t::debug("Got client_bind_request_t client %s:%hu", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));

			if (transaction_id_ != ntohl(client_bind_request.transaction_id))
			{
				log_t::error("Got bind request with incorrect transaction_id %lu!=%lu", transaction_id_, ntohl(client_bind_request.transaction_id));
				return false;
			}
			
			message_t message;
			message.type = message.client_bind_response;
			message.message.client_bind_response.transaction_id = htonl(transaction_id_);
			socket.set_target_address(peer_addr);

			int result = socket.send((uint8_t *)&message, sizeof(message_t));
			if (result == -1) {
				log_t::error("Failed sending client_bind_respone | %s", strerror(errno));
				return false;
			}
			return true;
		}

	};
}
	

