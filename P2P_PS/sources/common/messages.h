#include <stdint.h>





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
			client_bind_request = 2,
			client_bind_response = 3,
			ack = 4,
			heartbeat = 5
		} type;
	
		union message 
		{
			bind_request_t bind_request;
			bind_reply_t bind_reply;
			client_bind_request_t client_bind_request;
			client_bind_response_t client_bind_response;
			ack_t ack;
			heartbeat_t heatrbeat;
		} message;
	};

}