#pragma once
#include "connection_i.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <common/log.h>
#include <common/messages.h>

namespace network
{
    struct p2p_connector_t : 
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


		address_t stun_server_address;
		uint32_t transaction_id_ = 0;
	private:
		state state = disconnected;

	public:
	
		connect_result_e connect( socket_i & socket)
		{
			sockaddr_in stun_server_addr;
			bool connection_established = false;
			int result;
			message_t message;

			log_t::debug("Starting Connection to %hhu.%hhu.%hhu.%hhu:%hu",
				stun_server_address.ip_parts[0], stun_server_address.ip_parts[1],
				stun_server_address.ip_parts[2], stun_server_address.ip_parts[3],
				stun_server_address.port
			);

	

		

			//
			// Send Request to Server
			//

			memset(&stun_server_addr,0, sizeof(sockaddr_in));
		
			stun_server_addr.sin_addr.s_addr = htonl(stun_server_address.ip);
			stun_server_addr.sin_port = htons(stun_server_address.port);
			stun_server_addr.sin_family = AF_INET;
			socket.set_target_address(stun_server_addr);

			memset(&message,0, sizeof(message_t));
			message.type = message.bind_request;
			message.message.bind_request.transaction_id = htonl(transaction_id_);

			if (!socket.send((const uint8_t*)&message,sizeof(message))) {
				log_t::error("Failed when Sending stun request | %s", strerror(errno));
				return connect_result_e::connection_error;
			}

			result = socket.wait_receive((uint8_t*)&message, sizeof(message_t),5);
			if(result == -1 )
			{
				log_t::error("Error on receiving during connection establishment | %s", strerror(errno));
				return connect_result_e::connection_error;
			}else if (result == 0)
			{
				log_t::error("Server did not ack packet in time | %s", strerror(errno));
				return connect_result_e::connection_error;
			}

			state = state::waiting_for_stun_reply;

			uint32_t attempts_left = 10;
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
							try_connect_to_client(socket);
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
				case message.bind_reply:

					if (peer_addr.sin_addr.s_addr != stun_server_addr.sin_addr.s_addr ||
						peer_addr.sin_port != stun_server_addr.sin_port)
					{
						log_t::warning("Got bind reply from unknown host %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
						continue;
					}

					socket.set_target_address(message.message.bind_reply);

					if (try_connect_to_client(socket)) {
						state = state::waiting_for_client_reply;
						log_t::debug("Send client bind request to %s:%d waiting for response ", inet_ntoa(message.message.bind_reply.sin_addr), ntohs(message.message.bind_reply.sin_port));
						attempts_left = 3;
						continue;
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

		bool try_connect_to_client(socket_i & socket)
		{
			message_t message;
			message.type = message.client_bind_request;
			message.message.client_bind_request.transaction_id = htonl(transaction_id_);
			log_t::debug("Trying to connect to client %s:%d ", inet_ntoa(socket.get_target_address().sin_addr), ntohs(socket.get_target_address().sin_port));
			
			int result = socket.send((uint8_t *)&message, sizeof(message_t));
			if (result == -1) {
				log_t::error("Failed sending client_bind_respone | %s", strerror(errno));
				return false;
			}

			return true;
		}

		bool reply_to_bind_request(socket_i & socket, const message_t::client_bind_request_t & client_bind_request, sockaddr_in & peer_addr)
		{
			log_t::debug("Got client_bind_request_t client %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));

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
	

