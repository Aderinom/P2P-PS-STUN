#pragma once
#include "socket_i.h"
#include "port_manager.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ifaddrs.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <net/if.h>

extern network::port_manager_t portman;
namespace network
{
    class udp_socket_t : 
		public socket_i
	{
	public:
		udp_socket_t(){
		}

		~udp_socket_t()
		{
			if(initialized_)
			{
				close(socket_handle_);
			}
		}

		// socket_i
		bool init() override
		{
			if(initialized_) return true;
			socket_handle_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (socket_handle_ == -1)
			{
				log_t::error("Failed Creating Socket | %s", strerror(errno));
				return false;
			}
			initialized_ = true;
			return true;
		}
                   
		int bind(const sockaddr_in & addr) override
		{
			if(!initialized_) return false;
			return ::bind(socket_handle_, (const sockaddr *)&addr, sizeof(sockaddr_in));		
		}

		uint16_t bind_by_portman(const uint16_t base_port = 0) override
		{
			if(!initialized_) return false;
			sockaddr_in addr;
			memset(&addr,0,sizeof(sockaddr_in));

			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = htonl(INADDR_ANY);

			uint16_t port;
			if(base_port)
			{
				port = portman.assign_next_from_base(base_port);
			}
			else
			{
				port = portman.assign_next();
			}

			addr.sin_port = htons(port);

			if(::bind(socket_handle_, (const sockaddr *)&addr, sizeof(sockaddr_in)) < 0)
			{
				return 0;
			}
			else
			{
				return port;
			}
		}
		bool send(const uint8_t* buffer, uint32_t buffer_size) const override 
		{
			if(sendto(socket_handle_, (const char*)buffer, buffer_size, 0, (sockaddr*)&target_address_, sizeof(sockaddr_in)) == -1)
			{
				return false;
			}else{
				return true;
			};
		};

		int receive(uint8_t* buffer, uint32_t max_size) const override 
		{
			socklen_t len;
			return recvfrom(socket_handle_, (char*)buffer, max_size, 0, (struct sockaddr*)&target_address_, &len);
		};

		int wait_receive(uint8_t* buffer, uint32_t max_size, uint32_t timeout_sec ) const override 
		{
			timeval timeout = { timeout_sec, 0 };
			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(socket_handle_, &rfds);

			int result = select(socket_handle_ + 1, &rfds, NULL, NULL, &timeout);
			if (result <= 0) return result;
	
			socklen_t len;
			return recvfrom(socket_handle_, (char*)buffer, max_size, 0, (struct sockaddr*)&peer_address, &len);		
		};

		void set_target_address_(const sockaddr_in & target_address) 
		{
			target_address_ = target_address;
		};

		void set_target_address(const sockaddr_in & target_address) override
		{
			target_address_ = target_address;
		};

		const sockaddr_in& get_target_address() const override 
		{
			return target_address_;
		};
		const sockaddr_in& get_peer_address() const override
		{
			return peer_address;
		};
		const sockaddr_in& get_own_address() const override
		{
			if(own_address_.sin_addr.s_addr == 0) refresh_own_ipv4();
			return own_address_;
		};

		int get_socket() const
		{
			return socket_handle_;
		};

		socket_i::protocol get_protocol_type() const
		{
			return socket_i::udp;
		};

	private:
		//Not Properly implemented! Just gets any IP addr, not the one the socket is actualy bound to
		void refresh_own_ipv4() const
		{
			struct ifaddrs * ifAddrStruct=NULL;
			struct ifaddrs * ifa=NULL;

			getifaddrs(&ifAddrStruct);

			for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
				if (!ifa->ifa_addr ||
					!(ifa->ifa_flags & IFF_RUNNING) ||
					(ifa->ifa_flags & IFF_LOOPBACK)
				) continue;

				if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
					// is a valid IP4 Address
					own_address_=*((struct sockaddr_in *)ifa->ifa_addr);
					return;
				} 
				
			}
			if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);

			return;
		}

		bool initialized_ = false;
		sockaddr_in target_address_= {0};
		mutable sockaddr_in own_address_ = {0};
		mutable sockaddr_in peer_address = {0};

		int socket_handle_ = 0;
		uint32_t transaction_id_ = 0;
		bool quit_hb_ = false;
	};
}
