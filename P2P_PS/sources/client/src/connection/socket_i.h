#pragma once
#include <common/connection_types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>

#include <unistd.h>


namespace network
{
    struct socket_i
	{
		enum protocol
		{
			udp,
			tcp
		};

		virtual bool init() = 0;
		virtual bool send(const uint8_t* buffer, uint32_t buffer_size) const = 0;
		virtual int receive(uint8_t* buffer, uint32_t max_size) const = 0;
		virtual int wait_receive(uint8_t* buffer, uint32_t max_size, uint32_t timeout_sec ) const = 0;

		virtual void set_target_address( const sockaddr_in & target_address) = 0;

		virtual const sockaddr_in& get_target_address() const  = 0;
		virtual const sockaddr_in& get_own_address() const = 0;
		virtual const sockaddr_in& get_peer_address() const = 0;
		
		virtual protocol get_protocol_type() const = 0;
	};
}