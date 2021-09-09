#pragma once
#include <stdint.h>
#include <netinet/in.h>



///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Definitions for communication between different clients
//	Does not work if endianess of systems is different
//





namespace network
{
	struct message_t
	{

		struct bind_request_t
		{
			uint32_t transaction_id;
		};

		typedef sockaddr_in bind_reply_t;

		struct ps_bind_request_t
		{
			uint32_t transaction_id;
		};

		struct ps_bind_reply_t
		{
			sockaddr_in addr_one;
			sockaddr_in addr_two;
			uint64_t T1;
			uint64_t T2;
		};

		struct client_bind_request_t
		{
			uint32_t transaction_id;
		};

		struct client_bind_response_t
		{
			uint32_t transaction_id;
		};

		struct ack_t
		{
		};

		struct heartbeat_t
		{
		};

		enum type_t : uint8_t
		{
			bind_request = 0,
			bind_reply = 1,
			ps_bind_request = 2,
			ps_bind_reply = 3,
			client_bind_request = 4,
			client_bind_response = 5,
			ack = 6,
			heartbeat = 7
		} type;
	
		union message 
		{
			bind_request_t bind_request;
			bind_reply_t bind_reply;
			ps_bind_request_t ps_bind_request;
			ps_bind_reply_t ps_bind_reply;
			client_bind_request_t client_bind_request;
			client_bind_response_t client_bind_response;
			ack_t ack;
			heartbeat_t heatrbeat;
		} message;
	};

}