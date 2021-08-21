#pragma once
#include <common/connection_types.h>

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
#include <map>

extern bool sig_close;

namespace network
{
	class udp_server_t
	{
	public:
		udp_server_t()
		{
		}

		~udp_server_t()
		{
		}

		bool start(uint16_t port = 3333) {
			log_t::debug("Starting to listen port %hu", port);
			int result;

			socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (socket_handle == -1)
			{
				log_t::error("Failed Creating Socket | %s", strerror(errno));
				return 0;
			}

			bind_address.sin_family = AF_INET;
			bind_address.sin_port = htons(port);
			bind_address.sin_addr.s_addr = htonl(INADDR_ANY);

			result = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (result == -1)
			{
				log_t::error("Failed Creating Socket | %s", strerror(errno));
				return 0;
			}

			if (bind(socket_handle, (sockaddr*)&bind_address, sizeof(sockaddr_in)) > 0)
			{
				log_t::error("Failed Binding Socket | %s", strerror(errno));
				return connect_result_e::connection_error;
			}

			while (true && !sig_close)
			{
				sockaddr_in peer_addr;
				socklen_t len;
				message_t msg;
				result = recvfrom(socket_handle, &msg, sizeof(message_t), 0, (struct sockaddr*)(&peer_addr), (socklen_t*)&len);
				if (result == -1) {
					log_t::error("Eror on reading socket | %s", strerror(errno));
					return 0;
				}

				switch (msg.type)
				{
				case msg.bind_request:
					{
						
						message_t::bind_request_t& req = msg.message.bind_request;
						uint32_t transaction_id = ntohl(req.transaction_id);
						auto a = transaction_client_map.find(transaction_id);
						
						memset(&msg, 0, sizeof(msg));
						msg.type = msg.ack;
						sendto(socket_handle, (const char*)&msg, sizeof(msg), 0, (sockaddr*)&peer_addr, sizeof(sockaddr_in));

						if (a == transaction_client_map.end())
						{
							log_t::log("Got new transaction %lu for client %s:%hu", transaction_id, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
							transaction_client_map.insert(std::pair<const uint32_t, sockaddr_in>(transaction_id, std::move(peer_addr)));
						}
						else {

							sockaddr_in & copeer_addr = a->second;

							char peer_addr_string[20];
							snprintf(peer_addr_string, 20, "%s",inet_ntoa(peer_addr.sin_addr));

							log_t::log("Match %lu for clients %s:%hu | %s:%hu", transaction_id, 
								peer_addr_string, ntohs(peer_addr.sin_port),
								inet_ntoa(copeer_addr.sin_addr), ntohs(copeer_addr.sin_port)
							);

							msg.type = msg.bind_reply;
							msg.message.bind_reply = copeer_addr;

				
							log_t::debug("Sending bind reply to  %s:%hu | Connect to : %s:%hu", 
								peer_addr_string, ntohs(peer_addr.sin_port),
							 	inet_ntoa(copeer_addr.sin_addr), ntohs(copeer_addr.sin_port)
							);

							result = sendto(socket_handle, (const char*)&msg, sizeof(msg), 0, (sockaddr*)&peer_addr, sizeof(sockaddr_in));
							if (result == -1) {
								log_t::error("Failed when Sending bind reply to  %s:%hu | %s", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port),strerror(errno));
							}

							msg.message.bind_reply = peer_addr;
				
							log_t::debug("Sending bind reply to  %s:%hu | Connect to : %s:%hu",
								inet_ntoa(copeer_addr.sin_addr), ntohs(copeer_addr.sin_port), 
								peer_addr_string, ntohs(peer_addr.sin_port)
							);

							result = sendto(socket_handle, (const char*)&msg, sizeof(msg), 0, (sockaddr*)&copeer_addr, sizeof(sockaddr_in));
							if (result == -1) {
								log_t::error("Failed when Sending bind reply to  %s:%hu | %s", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port), strerror(errno));
							}

							transaction_client_map.erase(a);
						}
					}
					break;
				case msg.heartbeat:
					log_t::debug("Received Heartbeat from | %s:%hu", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
					memset(&msg, 0, sizeof(msg));
					msg.type = msg.ack;
					sendto(socket_handle, (const char*)&msg, sizeof(msg), 0, (sockaddr*)&peer_addr, sizeof(sockaddr_in));

					break;
				default:
					log_t::warning("Received unexpected packet - %hhu from | %s:%hu", msg.type, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
					break;
				}
			}
			return 1;
		}



	private:
		sockaddr_in bind_address;
		int socket_handle;
		uint64_t transaction_id_;


		std::map<uint32_t, sockaddr_in> transaction_client_map;

	};



}