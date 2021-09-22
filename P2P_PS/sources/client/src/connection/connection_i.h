#pragma once
#include "socket_i.h"
#include <common/connection_types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>

#include <unistd.h>


namespace network
{

	struct connector_i
	{
		virtual ~connector_i(){};
		virtual connect_result_e connect( socket_i & socket ) = 0;
	};

	struct connection_i
	{
		enum protocol
		{
			udp,
			tcp
		};

		enum state
		{
			invalid,
			disconnected,
			connecting,
			connected,
		};
		
		virtual ~connection_i(){};
		virtual connect_result_e connect(connector_i & connector_algorithm) = 0;
		virtual bool send(const uint8_t* buffer, uint32_t buffer_size) const = 0;
		virtual uint32_t receive(uint8_t* buffer, uint32_t max_size) const = 0;
		virtual uint32_t wait_receive(uint8_t* buffer, uint32_t max_size, uint32_t timeout_sec ) const = 0;

		virtual socket_i& get_socket()  = 0;

	};
}
	