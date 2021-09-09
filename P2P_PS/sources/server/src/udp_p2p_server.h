#pragma once
#include <common/connection_types.h>
#include <common/messages.h>
#include <common/log.h>
#include <common/time_helper.h>

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>


#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <assert.h>
#include <map>

extern bool sig_close;

namespace network
{
	class udp_server_t
	{	
	public:
	struct ps_transaction_t
	{
		ps_transaction_t(sockaddr_in & addr_1, uint64_t timestamp_1):
			cl1_sk1(addr_1),
			cl1_ts1(timestamp_1)
		{

		}

		sockaddr_in cl1_sk1 = {0,0,0};
		sockaddr_in cl1_sk2 = {0,0,0};

		uint64_t cl1_ts1 = 0;
		uint64_t cl1_ts2 = 0;

		sockaddr_in cl2_sk1 = {0,0,0};
		sockaddr_in cl2_sk2 = {0,0,0};

		uint64_t cl2_ts1 = 0;
		uint64_t cl2_ts2 = 0;
	};

	private:
		int socket_[2];
		
		sockaddr_in bind_address;
		std::map<uint32_t, sockaddr_in> transaction_client_map;
		std::map<uint32_t, ps_transaction_t> ps_transaction_client_map;

	public:
		udp_server_t()
		{
		}

		~udp_server_t()
		{
		}

		bool start(uint16_t port1 = 3333, uint16_t port2 = 3334) {
			log_t::debug("Starting to listen port %hu and %hu", port1, port2);
			int result;


			///
			/// Create Socket 1 and bind to Port 1
			///

			socket_[0] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (socket_[0] == -1)
			{
				log_t::error("Failed Creating Socket1 | %s", strerror(errno));
				return 0;
			}

			bind_address.sin_family = AF_INET;
			bind_address.sin_port = htons(port1);
			bind_address.sin_addr.s_addr = htonl(INADDR_ANY);

			if (bind(socket_[0], (sockaddr*)&bind_address, sizeof(sockaddr_in)) > 0)
			{
				log_t::error("Failed Binding Socket1 | %s", strerror(errno));
				return 0;
			}

			///
			/// Create Socket 2 and bind to Port 2
			///

			socket_[1] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (socket_[1] == -1)
			{
				log_t::error("Failed Creating Socket2 | %s", strerror(errno));
				return 0;
			}

			bind_address.sin_family = AF_INET;
			bind_address.sin_port = htons(port2);
			bind_address.sin_addr.s_addr = htonl(INADDR_ANY);

			if (bind(socket_[1], (sockaddr*)&bind_address, sizeof(sockaddr_in)) > 0)
			{
				log_t::error("Failed Binding Socket2 | %s", strerror(errno));
				return 0;
			}

			///
			/// Start Receiving
			///

			while (true && !sig_close)
			{
				sockaddr_in peer_addr;
				socklen_t len = sizeof(sockaddr_in);
				message_t msg;
			
				fd_set readfds;

				int socket_handle;
				uint32_t i;
				int status;

				FD_ZERO(&readfds);
				FD_SET(socket_[0], &readfds);
				FD_SET(socket_[1], &readfds);
				int maxfd = socket_[0] > socket_[1] ? socket_[0] : socket_[1];
				
				status = select(maxfd + 1, &readfds, NULL, NULL, NULL);
				if (status < 0)
				{
					log_t::error("Failed reading sockets | %s", strerror(errno));
					return 0;
				}

				for (i = 0; i < 2; i++)
				{
					if (FD_ISSET(socket_[i], &readfds)) {
						socket_handle = socket_[i];
						result = recvfrom(socket_handle, &msg, sizeof(message_t), 0, (struct sockaddr*)(&peer_addr), &len);
						if (result == -1) {
							log_t::error("Eror on reading socket | %s", strerror(errno));
							return 0;
						}

						switch (msg.type)
						{
						case msg.ps_bind_request:
						{
							{
								message_t::ps_bind_request_t req = msg.message.ps_bind_request;
								memset(&msg, 0, sizeof(msg));
								msg.type = msg.ack;
								sendto(socket_handle, (const char*)&msg, sizeof(msg), 0, (sockaddr*)&peer_addr, sizeof(sockaddr_in));								
					
								uint32_t transaction_id = ntohl(req.transaction_id);
								auto it = ps_transaction_client_map.find(transaction_id);

								if (it == ps_transaction_client_map.end())
								{
									log_t::log("[%lu] NEW | client %s:%hu", transaction_id, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
									ps_transaction_client_map.insert(std::pair<const uint32_t, ps_transaction_t>(transaction_id, ps_transaction_t(peer_addr, get_time_in_ms())));
									break;
								}
								else 
								{	
									ps_transaction_t & transaction = it->second;
									if(transaction.cl1_ts1 + 30000 < get_time_in_ms())
									{
										log_t::log("[%lu] NEW | client %s:%hu", transaction_id, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
										memset(&transaction, 0 , sizeof(ps_transaction_t));
										transaction.cl1_ts1 = get_time_in_ms();
										transaction.cl1_sk1 = peer_addr;
										break;
									}
									
									if(transaction.cl1_sk1.sin_addr.s_addr == peer_addr.sin_addr.s_addr)
									{
										log_t::log("[%lu] ADD | cl1 port2 | %s:%hu//%hu", transaction_id, inet_ntoa(peer_addr.sin_addr), ntohs(transaction.cl1_sk1.sin_port) ,ntohs(peer_addr.sin_port));
										transaction.cl1_ts2 = get_time_in_ms();
										transaction.cl1_sk2  = peer_addr;
										break;
									}
									else if(transaction.cl2_sk1.sin_addr.s_addr == 0)
									{
										log_t::log("[%lu] ADD | cl2 port1 | %s:%hu", transaction_id, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
										transaction.cl2_ts1 = get_time_in_ms();
										transaction.cl2_sk1  = peer_addr;
									}
									else if(transaction.cl2_sk1.sin_addr.s_addr == peer_addr.sin_addr.s_addr)
									{
										log_t::log("[%lu] ADD | cl2 port2 | %s:%hu//%hu", transaction_id, inet_ntoa(peer_addr.sin_addr), ntohs(transaction.cl2_sk1.sin_port) ,ntohs(peer_addr.sin_port));
										transaction.cl2_ts2 = get_time_in_ms();
										transaction.cl2_sk2  = peer_addr;	
									}

									if(transaction.cl2_sk2.sin_addr.s_addr != 0 && transaction.cl1_sk2.sin_addr.s_addr != 0)
									{
										char peer_addr_string[20];
										snprintf(peer_addr_string, 20, "%s",inet_ntoa(transaction.cl1_sk2.sin_addr));
										log_t::log("[%lu] RDY | %s:%hu/%hu | %s:%hu/%hu" , transaction_id, 
												peer_addr_string, ntohs(transaction.cl1_sk1.sin_port), ntohs(transaction.cl1_sk2.sin_port),
												inet_ntoa(transaction.cl2_sk1.sin_addr), ntohs(transaction.cl2_sk1.sin_port), ntohs(transaction.cl2_sk2.sin_port));
										
									}else{
										break;
									}
									sockaddr_in & cl1_addr = transaction.cl1_sk1;
									sockaddr_in & cl2_addr = transaction.cl2_sk1;	

									msg.type = msg.ps_bind_reply;
									msg.message.ps_bind_reply.addr_one = transaction.cl2_sk1;
									msg.message.ps_bind_reply.addr_two = transaction.cl2_sk2;
									msg.message.ps_bind_reply.T1 =  transaction.cl2_ts1;
									msg.message.ps_bind_reply.T2 =  transaction.cl2_ts2;

									result = sendto(socket_[0], (const char*)&msg, sizeof(msg), 0, (sockaddr*)&cl1_addr, sizeof(sockaddr_in));
									if (result == -1) {
										log_t::error("[%lu] ERR | Sending bind reply to  %s:%hu | %s", inet_ntoa(cl1_addr.sin_addr), ntohs(cl1_addr.sin_port),strerror(errno));
									}

									msg.message.ps_bind_reply.addr_one = transaction.cl1_sk1;
									msg.message.ps_bind_reply.addr_two = transaction.cl1_sk2;
									msg.message.ps_bind_reply.T1 =  transaction.cl1_ts1;
									msg.message.ps_bind_reply.T2 =  transaction.cl1_ts2;
								
									result = sendto(socket_[0], (const char*)&msg, sizeof(msg), 0, (sockaddr*)&cl2_addr, sizeof(sockaddr_in));
									if (result == -1) {
										log_t::error("[%lu] ERR | Sending bind reply to  %s:%hu | %s", inet_ntoa(cl2_addr.sin_addr), ntohs(cl2_addr.sin_port), strerror(errno));
									}

									ps_transaction_client_map.erase(it);
								}
							}

						}break;
						case msg.bind_request:
							{

								message_t::bind_request_t& req = msg.message.bind_request;

								memset(&msg, 0, sizeof(msg));
								msg.type = msg.ack;
								sendto(socket_[0], (const char*)&msg, sizeof(msg), 0, (sockaddr*)&peer_addr, sizeof(sockaddr_in));
		
								uint32_t transaction_id = ntohl(req.transaction_id);
								auto a = transaction_client_map.find(transaction_id);
								
					

								if (a == transaction_client_map.end())
								{
									log_t::log("[%lu] NEW | client %s:%hu", transaction_id, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
									transaction_client_map.insert(std::pair<const uint32_t, sockaddr_in>(transaction_id, std::move(peer_addr)));
								}
								else {

									sockaddr_in & copeer_addr = a->second;

									char peer_addr_string[20];
									snprintf(peer_addr_string, 20, "%s",inet_ntoa(peer_addr.sin_addr));

									log_t::log("[%lu] RDY | clients %s:%hu | %s:%hu", transaction_id, 
										peer_addr_string, ntohs(peer_addr.sin_port),
										inet_ntoa(copeer_addr.sin_addr), ntohs(copeer_addr.sin_port)
									);

									msg.type = msg.bind_reply;
									msg.message.bind_reply = copeer_addr;

						
									result = sendto(socket_[0], (const char*)&msg, sizeof(msg), 0, (sockaddr*)&peer_addr, sizeof(sockaddr_in));
									if (result == -1) {
										log_t::error("[%lu] ERR | Sending bind reply to  %s:%hu | %s", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port),strerror(errno));
									}

									msg.message.bind_reply = peer_addr;
						
	
									result = sendto(socket_[0], (const char*)&msg, sizeof(msg), 0, (sockaddr*)&copeer_addr, sizeof(sockaddr_in));
									if (result == -1) {
										log_t::error("[%lu] ERR | Sending bind reply to  %s:%hu | %s", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port), strerror(errno));
									}

									transaction_client_map.erase(a);
								}
							}
							break;
						case msg.heartbeat:
							//log_t::debug("[%lu] HB |  Heartbeat from | %s:%hu", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
							memset(&msg, 0, sizeof(msg));
							msg.type = msg.ack;
							sendto(socket_[0], (const char*)&msg, sizeof(msg), 0, (sockaddr*)&peer_addr, sizeof(sockaddr_in));

							break;
						default:
							log_t::warning("Received unexpected packet - %hhu from | %s:%hu", msg.type, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
							break;
						}
					}
				}
			}
			return 1;
		}

	};




}