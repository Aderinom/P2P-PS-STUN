#pragma once
#include "i_connection_t.h"

#include <common/log.h>

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


#include<common/ministun.h>


namespace network
{
	class udp_connection_t
	{
	public:
		udp_connection_t()
		{

		}

		~udp_connection_t()
		{
		}

		// i_connection_t
		connect_result_e p2p_connect(std::string connection_hash, address_t stun_server_address)
		{
			sockaddr_in stun_server_addr;
			socklen_t sockaddr_len = sizeof(sockaddr_in);

			log_t::debug("Starting Connection to %hhu.%hhu.%hhu.%hhu:%hu",
				stun_server_address.ip_parts[0], stun_server_address.ip_parts[1],
				stun_server_address.ip_parts[2], stun_server_address.ip_parts[3],
				stun_server_address.port
			);

			stun_server_addr.sin_addr.s_addr = htonl(stun_server_address.ip);
			stun_server_addr.sin_port = htons(stun_server_address.port);
			stun_server_addr.sin_family = AF_INET;


			socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (socket_handle == -1)
			{
				log_t::error("Failed Creating Socket | %s", strerror(errno));
				return connect_result_e::connection_error;
			}


			own_address.sin_addr.s_addr = htonl(INADDR_ANY);
			own_address.sin_port = htons(0);
			own_address.sin_family = AF_INET;

			if (bind(socket_handle, (sockaddr*)&own_address, sockaddr_len) >= 0)
			{
				log_t::error("Failed Binding Socket | %s", strerror(errno));
				return connect_result_e::connection_error;
			}

			int result;

			//
			// Send Request to Server
			//

			transaction_id = 1337;
			message_t message;
			message.type = message.bind_request;
			message.message.bind_request.transaction_id = htonl(transaction_id);

			result = sendto(socket_handle, (const char*)&message, sizeof(message), 0, (sockaddr*)&stun_server_addr, sockaddr_len);
			if (result == -1) {
				log_t::error("Failed when Sending stun request | %s", strerror(errno));
				return connect_result_e::connection_error;
			}

			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(socket_handle, &rfds);
			timeval timeout = { STUN_TIMEOUT, 0 };


			bool connection_established = false;
			uint32_t attempts_left = 10;
			sockaddr_in peer_addr;
			socklen_t socklen;

			while (!connection_established && attempts_left)
			{

				//
				// Await reqsponse from server
				//

				result = select(socket_handle + 1, &rfds, NULL, NULL, &timeout);
				if (result == -1) {
					log_t::debug("Timeout or Error while waiting for stun reply | %s", strerror(errno));
					attempts_left--;
					continue;
				}

				//
				// read response from server
				//

				result = recvfrom(socket_handle, (char*)&message, sizeof(message), 0, (struct sockaddr*)&peer_addr, &socklen);
				if (result == -1) {
					log_t::error("Error while receiving socket | %s", strerror(errno));
					return connect_result_e::connection_error;
				}


				if (message.type == message.bind_reply) {

					if (peer_addr.sin_addr.s_addr != stun_server_addr.sin_addr.s_addr ||
						peer_addr.sin_port != stun_server_addr.sin_port)
					{
						log_t::warning("Got bind reply from unknown host %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
						continue;
					}

					if (try_connect_to_client(message.message.bind_reply)) {
						log_t::debug("Successfully connected to client %s:%d", inet_ntoa(target_address.sin_addr), ntohs(target_address.sin_port));
						connection_established = true;
						continue;
					}

				}
				else if (message.type == message.client_bind_request)
				{
					if (reply_to_bind_request(message.message.client_bind_request, peer_addr)) {
						log_t::debug("Send client bind request to %s:%d waiting for response ", inet_ntoa(target_address.sin_addr), ntohs(target_address.sin_port));
					};
				}
				else if(message.type == message.client_bind_response)
				{
					if (peer_addr.sin_addr.s_addr != target_address.sin_addr.s_addr ||
						peer_addr.sin_port != target_address.sin_port)
					{
						log_t::warning("Got client_bind_response from unknown host %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
						continue;
					}

					log_t::debug("Successfully connected to peer  %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
					connection_established = true;
					continue;
				}
				else
				{
					log_t::warning("Received unexcpected packet - %hhu", message.type);
				}
				
				attempts_left--;
			}

		}
			
		virtual bool send(uint8_t* buffer, uint32_t buffer_size);
		virtual uint32_t receive(uint8_t* buffer, uint32_t max_size);


	private:

		bool try_connect_to_client(const message_t::bind_reply_t & bind_reply)
		{
			message_t message;
			message.type = message.client_bind_request;

			log_t::debug("Got bind reply from STUN trying to connect to client %s:%d", inet_ntoa(bind_reply.sin_addr), ntohs(bind_reply.sin_port));

			int result = sendto(socket_handle, (const char*)&message, sizeof(message), 0, (sockaddr*)&bind_reply, sizeof(sockaddr_in));
			if (result == -1) {
				log_t::error("Failed sending client_bind_respone | %s", strerror(errno));
				return false;
			}

			return true;
		}

		bool reply_to_bind_request(const message_t::client_bind_request_t & client_bind_request, sockaddr_in & peer_addr)
		{
			log_t::debug("Go tclient_bind_request_t client %s:%d", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));

			if (transaction_id != ntohl(client_bind_request.transaction_id))
			{
				log_t::error("Got bind request with incorrect transaction_id %lu!=%lu", transaction_id, ntohl(client_bind_request.transaction_id));
				return false;
			}
			
			message_t message;
			message.type = message.client_bind_response;

			int result = sendto(socket_handle, (const char*)&message, sizeof(message), 0, (sockaddr*)&peer_addr, sizeof(sockaddr_in));
			if (result == -1) {
				log_t::error("Failed sending client_bind_respone | %s", strerror(errno));
				return false;
			}
			target_address = peer_addr;
			return true;
		}

		sockaddr_in target_address;
		sockaddr_in own_address;
		int socket_handle;
		uint64_t transaction_id;

	};





}




