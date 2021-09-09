#pragma once
#include "connection_i.h"
#include "udp_socket.h"
#include <common/connection_types.h>

#include <common/log.h>

#include <common/messages.h>
#include <assert.h>




namespace network
{	
	
	class udp_connection : 
		public connection_i
	{
	private:
		udp_socket_t socket;
		
	public:
		udp_connection() {
			if(!socket.init())
			{
				assert("socket failed init");
			}
			
		}
		~udp_connection() {}

		virtual connect_result_e connect(connector_i & connector_algorithm) override
		{
			return connector_algorithm.connect(socket);
		};

		inline virtual bool send(const uint8_t* buffer, uint32_t buffer_size) const override
		{
			return socket.send(buffer, buffer_size);
		};
		
		inline virtual uint32_t receive(uint8_t* buffer, uint32_t max_size) const override		
		{
			return socket.receive(buffer, max_size);
		};

		inline virtual uint32_t wait_receive(uint8_t* buffer, uint32_t max_size, uint32_t timeout_sec ) const override	
		{
			return socket.wait_receive(buffer, max_size, timeout_sec);
		};

		virtual socket_i& get_socket() override
		{
			return socket;
		};

	};


}


