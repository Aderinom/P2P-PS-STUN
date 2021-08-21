#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <common/log.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace network
{

	struct address_t
	{
		union 
		{
			uint32_t ip;
			uint8_t ip_parts[4];
		};

		uint16_t port;

		static int parse_ip(char* ip_addr)
		{
			sockaddr_in aip;
			if (inet_aton(ip_addr, &aip.sin_addr) == 0)
			{
				log_t::error("Failed parsing ip_addr %s",ip_addr);
				return 0;
			}


			return ntohl(aip.sin_addr.s_addr);
		}
	};
	

	enum connect_result_e
	{
		invalid,
		connection_failed_no_server_match_in_time,
		connection_successful,
		connection_failed,
		connection_error
	};

}
