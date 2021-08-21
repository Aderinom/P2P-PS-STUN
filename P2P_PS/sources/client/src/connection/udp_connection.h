#pragma once
#include <common/connection_types.h>

#include <common/log.h>

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <common/messages.h>
#include <assert.h>

#include <thread>

#include<common/ministun.h>


namespace network
{
	class udp_connection_t
	{
	public:
		enum state_t
		{
			disconnected,
			waiting_for_server_answer,
			waiting_for_client_answer,
			connected
		};

		udp_connection_t()
		{

		}

		~udp_connection_t()
		{
		}

		// i_connection_t
		connect_result_e p2p_connect(uint32_t transaction_id, address_t stun_server_address)
		{
		
			sockaddr_in stun_server_addr;
			socklen_t sockaddr_len = sizeof(sockaddr_in);

			log_t::debug("Starting Connection to %hhu.%hhu.%hhu.%hhu:%hu",
				stun_server_address.ip_parts[0], stun_server_address.ip_parts[1],
				stun_server_address.ip_parts[2], stun_server_address.ip_parts[3],
				stun_server_address.port
			);

			memset(&stun_server_addr,0, sizeof(sockaddr_in));
			
			stun_server_addr.sin_addr.s_addr = htonl(stun_server_address.ip);
			stun_server_addr.sin_port = htons(stun_server_address.port);
			stun_server_addr.sin_family = AF_INET;


			socket_handle_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (socket_handle_ == -1)
			{
				log_t::error("Failed Creating Socket | %s", strerror(errno));
				return connect_result_e::connection_error;
			}


			own_address_.sin_addr.s_addr = htonl(INADDR_ANY);
			own_address_.sin_port = htons(0);
			own_address_.sin_family = AF_INET;

			int result;

			//
			// Send Request to Server
			//

			transaction_id_ = transaction_id;
			message_t message;
			message.type = message.bind_request;
			message.message.bind_request.transaction_id = htonl(transaction_id_);

			result = sendto(socket_handle_, (const char*)&message, sizeof(message), 0, (sockaddr*)&stun_server_addr, sockaddr_len);
			if (result == -1) {
				log_t::error("Failed when Sending stun request | %s", strerror(errno));
				return connect_result_e::connection_error;
			}
			state_ = state_t::waiting_for_server_answer;
			bool connection_established = false;
			uint32_t attempts_left = 10;
			sockaddr_in peer_addr;
			socklen_t socklen;

			while (!connection_established && attempts_left)
			{
		
				//
				// Await reqsponse from server
				//


				//
				// Heartbeat - not required
				//

				//if(last_attempts != attempts_left)
				//{
					//memset(&message, 0, sizeof(network::message_t));
					//message.type = message.heartbeat;
					//log_t::debug("send hb");
				//	result = sendto(socket_handle_, (const char*)&message, sizeof(network::message_t), 0, (sockaddr*)&stun_server_addr, sizeof(sockaddr_in));
				//	if (result == -1) {
					//	log_t::error("Failed when Sending heartbeat | %s", strerror(errno));
					//}	
				//	last_attempts = attempts_left;
				//}
				
				
				timeval timeout = {3, 0 };
				FD_ZERO(&rfds_);
				FD_SET(socket_handle_, &rfds_);

				result = select(socket_handle_ + 1, &rfds_, NULL, NULL, &timeout);
				if (result == 0) {
	
					switch (state_)
					{
						case state_t::waiting_for_server_answer :
							log_t::debug("Still waiting for STUN Server Reply");
							break;
						case state_t::waiting_for_client_answer :
							log_t::debug("Still waiting for Peer Client Reply");
							try_connect_to_client();
							break;

						default:
							break;
					}
					
					attempts_left--;
					continue;
				}
				else if (result < 0) {
					log_t::debug("Error while waiting for stun reply | %s", strerror(errno));
					attempts_left--;
					continue;
				}
				
				//
				// read response from server
				//
				result = recvfrom(socket_handle_, (char*)&message, sizeof(message), 0, (struct sockaddr*)&peer_addr, &socklen);
				if (result == -1) {
					log_t::error("Error while receiving socket | %s", strerror(errno));
					return connect_result_e::connection_error;
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
					target_address_ = message.message.bind_reply;

					if (try_connect_to_client()) {
						state_ = state_t::waiting_for_client_answer;
						log_t::debug("Send client bind request to %s:%d waiting for response ", inet_ntoa(target_address_.sin_addr), ntohs(target_address_.sin_port));
						attempts_left = 2;
						continue;
					}

					break;
				case message.client_bind_request:

					if (reply_to_bind_request(message.message.client_bind_request, peer_addr)) {
						log_t::success("Successfully connected to peer %s:%d", inet_ntoa(target_address_.sin_addr), ntohs(target_address_.sin_port));
						connection_established = true;
						state_ = state_t::connected;
						continue;
					};

					break;
				case message.client_bind_response:

					if (peer_addr.sin_addr.s_addr != target_address_.sin_addr.s_addr ||
						peer_addr.sin_port != target_address_.sin_port)
					{
						log_t::warning("Got client_bind_response from unknown host %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
						continue;
					}
					state_ = state_t::connected;
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

			switch (state_)
			{
			case state_t::connected:
					return connect_result_e::connection_successful;
				break;
			case state_t::waiting_for_server_answer:
					log_t::log("Failed p2p Connection because server did not answer");
					return connect_result_e::connection_failed_no_server_match_in_time;
				break;
			default:
					log_t::log("Failed p2p Connection");
					return connect_result_e::connection_failed;
				break;
			}
	
				
	
	

		}
			
		bool send(const uint8_t* buffer, uint32_t buffer_size) {
			sendto(socket_handle_, (const char*)buffer, buffer_size, 0, (sockaddr*)&target_address_, sizeof(sockaddr_in));
			return true;
		};

		uint32_t wait_receive(uint8_t* buffer, uint32_t max_size, uint32_t timeout_sec ) {
			timeval timeout = { timeout_sec, 0 };

			FD_ZERO(&rfds_);
			FD_SET(socket_handle_, &rfds_);

			int result = select(socket_handle_ + 1, &rfds_, NULL, NULL, &timeout);
			if (result <= 0) return result;
	
			socklen_t len;
			return recvfrom(socket_handle_, (char*)buffer, max_size, 0, (struct sockaddr*)&target_address_, &len);		
		};

		uint32_t wait_till_receive(uint8_t* buffer, uint32_t max_size) {
			socklen_t len;
			return recvfrom(socket_handle_, (char*)buffer, max_size, 0, (struct sockaddr*)&target_address_, &len);
		};

		const sockaddr_in& get_target_address() const 
		{
			return target_address_;
		};
		const sockaddr_in&  get_own_address() const
		{
			if(own_address_.sin_addr.s_addr == 0) refresh_own_ipv4();
			return own_address_;
		};


	private:

		void refresh_own_ipv4()
		{
			struct ifaddrs * ifAddrStruct=NULL;
			struct ifaddrs * ifa=NULL;
			void * tmpAddrPtr=NULL;

			getifaddrs(&ifAddrStruct);

			for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
				if (!ifa->ifa_addr) {
					continue;
				}

				if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
					// is a valid IP4 Address
					own_address_=*((struct sockaddr_in *)ifa->ifa_addr);
					return;
				} 
				
			}
			if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);

			return;
		}

		bool try_connect_to_client()
		{
			message_t message;
			message.type = message.client_bind_request;
			message.message.client_bind_request.transaction_id = htonl(transaction_id_);
			log_t::debug("Trying to connect to client %s:%d ", inet_ntoa(target_address_.sin_addr), ntohs(target_address_.sin_port));
			
			int result = sendto(socket_handle_, (const char*)&message, sizeof(message), 0, (sockaddr*)&target_address_, sizeof(sockaddr_in));
			if (result == -1) {
				log_t::error("Failed sending client_bind_respone | %s", strerror(errno));
				return false;
			}

			return true;
		}

		bool reply_to_bind_request(const message_t::client_bind_request_t & client_bind_request, sockaddr_in & peer_addr)
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

			int result = sendto(socket_handle_, (const char*)&message, sizeof(message), 0, (sockaddr*)&peer_addr, sizeof(sockaddr_in));
			if (result == -1) {
				log_t::error("Failed sending client_bind_respone | %s", strerror(errno));
				return false;
			}
			target_address_ = peer_addr;
			return true;
		}


		state_t state_ = disconnected;
		fd_set rfds_;
		sockaddr_in target_address_= {0,0,0};
		sockaddr_in own_address_ = {0,0,0};
		int socket_handle_ = 0;
		uint32_t transaction_id_ = 0;
		bool quit_hb_ = false;
	};





}




