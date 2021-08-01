#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <common/log.h>

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
	};
	

	enum connect_result_e
	{
		invalid,
		connection_successful,
		connection_failed,
		connection_error
	};

	struct i_connection_t
	{
		virtual ~i_connection_t() {};
		virtual connect_result_e connect(address_t ip_address, uint16_t port) = 0;
		virtual bool send(uint8_t* buffer, uint32_t buffer_size) = 0;
		virtual uint32_t receive(uint8_t* buffer, uint32_t max_size) = 0;

	};
}
