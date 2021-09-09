#include <common/log.h>
#include <common/event.h>

#include "connection/udp_connection.h"
#include "connection/p2p_connector.h"
#include "connection/p2p_ps_connector.h"

#include <thread>
#include <atomic>
#include <vector>
#include <condition_variable>



loglevel_e loglevel = loglevel_e::DEBUG;
bool quit = false;

void usage()
{
	printf("Innvalid Arguments\n");
	printf("p2p_client <target_ip> <target_port> <statistic_run_count> <use portscanning>\n");
	printf("p2p_client 10.10.10.10 3333 100 1\n");
};



struct statistical_runner
{
	statistical_runner():
		successfull_runs(0),
		failed_runs(0),
		programm_mistakes(0),
		current_transaction_id(0)
	{

	}

	void run(network::address_t& address, uint32_t runs)
	{	
		
		address_ = address;
		run_count = runs;
		log_t::log("Starting Statistical Client on %u runs", runs);
		uint32_t processor_count =  50;

		if(processor_count == 0) 
		{
			log_t::error("Could not get processor_count!");
			processor_count = 20;
		}
		loglevel = ERROR;
		for (uint32_t i = 0; i < processor_count; i++)
		{
			thread_list.push_back(std::thread(&statistical_runner::test_thread_main,this));
		}
		
		quit_.wait();

		for (auto it = thread_list.begin(); it != thread_list.end(); ++it)
		{
			it->join();
		};

		printf("\n\n\n");
		printf("------- STAT RUN COMPLETE -------\n");
		printf("Mode      : Basic Holepunching\n");
		printf("Runs      : %4u\n" , runs);
		printf("Succeeded : %4u\n" , successfull_runs.load(std::memory_order_relaxed));
		printf("Failed    : %4u\n" , failed_runs.load(std::memory_order_relaxed));
		printf("Errored	  : %4u\n" , programm_mistakes.load(std::memory_order_relaxed));
		printf("---------------------------------\n");
		printf("%% Success : %.3f                \n" , successfull_runs.load(std::memory_order_relaxed)/(double)(runs - programm_mistakes.load(std::memory_order_relaxed)));



		return;
	}



	void test_thread_main()
	{

		for (uint32_t i = current_transaction_id++; i < run_count; i = current_transaction_id++)
		{
			network::udp_connection conn;
			network::p2p_connector_t p2p_con;
			p2p_con.stun_server_address = address_;
			p2p_con.transaction_id_ = i;
			network::connect_result_e result = conn.connect(p2p_con);
		
			switch (result)
			{
			case network::connection_successful:
				log_t::statistic("Run %4u succeeded",i);
				successfull_runs ++;
				break;
			case network::connection_failed_no_server_match_in_time:
				log_t::statistic("Run %4u errored - test clients mismatching",i);
				programm_mistakes ++;
				break;
			case network::connection_error:
				log_t::statistic("Run %4u errored - internal error",i);
				programm_mistakes ++;
				break;
			case network::connection_failed:
				log_t::statistic("Run %4u failed",i);
				failed_runs++;
				break;
			default:
				break;
			}

		}

		quit_.signal();
	}

	network::address_t address_;
	uint32_t run_count = 0;

	std::atomic<uint32_t> successfull_runs;
	std::atomic<uint32_t> failed_runs;
	std::atomic<uint32_t> programm_mistakes;


	std::atomic<uint32_t> current_transaction_id;
	std::vector<std::thread> thread_list;
	event_t quit_;

};

struct normal_runner
{
	void run(network::address_t& address)
	{
		log_t::log("Starting Client");
		network::udp_connection conn;
		network::p2p_connector_t p2p_con;
		p2p_con.stun_server_address = address;
		p2p_con.transaction_id_ = 1337;
		network::connect_result_e result = conn.connect(p2p_con);
		
		if (result == network::connection_successful)
		{
			printf("1");
			std::thread heartbeater(thread_hb, std::ref(conn));
			
			while(!quit)
			{
				char input[254];
				conn.receive((uint8_t*)input,254);
			
				log_t::log("%s",input);
			}

			heartbeater.join();
		}else{
			printf("0");
		}
	}

	static void thread_hb(network::udp_connection & conn)
	{
		const sockaddr_in & myaddress = conn.get_socket().get_own_address();
		char msg[254];
		int len;
		len = snprintf(msg ,254,"%s | Hi!",inet_ntoa(myaddress.sin_addr));


		while(!quit)
		{
			sleep(3);
			conn.send((const uint8_t*)msg,len + 1);
		}
	}
};

struct statistical_runner_ps
{
	statistical_runner_ps():
		successfull_runs(0),
		failed_runs(0),
		programm_mistakes(0),
		current_transaction_id(0)
	{

	}

	void run(network::address_t& address, uint32_t runs)
	{	
		
		address_ = address;
		run_count = runs;
		log_t::log("Starting Statistical Client on %u runs", runs);
		uint32_t processor_count =  50;

		if(processor_count == 0) 
		{
			log_t::error("Could not get processor_count!");
			processor_count = 20;
		}
		loglevel = ERROR;
		for (uint32_t i = 0; i < processor_count; i++)
		{
			thread_list.push_back(std::thread(&statistical_runner_ps::test_thread_main,this));
		}
		
		quit_.wait();

		for (auto it = thread_list.begin(); it != thread_list.end(); ++it)
		{
			it->join();
		};

		printf("\n\n\n");
		printf("------- STAT RUN COMPLETE -------\n");
		printf("Mode      : Port Scanning Holepunch\n");
		printf("Runs      : %4u\n" , runs);
		printf("Succeeded : %4u\n" , successfull_runs.load(std::memory_order_relaxed));
		printf("Failed    : %4u\n" , failed_runs.load(std::memory_order_relaxed));
		printf("Errored	  : %4u\n" , programm_mistakes.load(std::memory_order_relaxed));
		printf("---------------------------------\n");
		printf("%% Success : %.3f                \n" , successfull_runs.load(std::memory_order_relaxed)/(double)(runs - programm_mistakes.load(std::memory_order_relaxed)));



		return;
	}

	void test_thread_main()
	{

		for (uint32_t i = current_transaction_id++; i < run_count; i = current_transaction_id++)
		{
			network::udp_connection conn;
			network::p2p_ps_connector_t p2p_con;
			p2p_con.stun_server_address1 = address_;
			p2p_con.stun_server_address2 = address_;
			//p2p_con.stun_server_address2.port++;

			p2p_con.transaction_id_ = i;
			network::connect_result_e result = conn.connect(p2p_con);
		
			switch (result)
			{
			case network::connection_successful:
				log_t::statistic("Run %4u succeeded",i);
				successfull_runs ++;
				break;
			case network::connection_failed_no_server_match_in_time:
				log_t::statistic("Run %4u errored - test clients mismatching",i);
				programm_mistakes ++;
				break;
			case network::connection_error:
				log_t::statistic("Run %4u errored - internal error",i);
				programm_mistakes ++;
				break;
			case network::connection_failed:
				log_t::statistic("Run %4u failed",i);
				failed_runs++;
				break;
			default:
				break;
			}

		}

		quit_.signal();
	}

	network::address_t address_;
	uint32_t run_count = 0;

	std::atomic<uint32_t> successfull_runs;
	std::atomic<uint32_t> failed_runs;
	std::atomic<uint32_t> programm_mistakes;


	std::atomic<uint32_t> current_transaction_id;
	std::vector<std::thread> thread_list;
	event_t quit_;

};

struct normal_runner_ps
{
	void run(network::address_t& address)
	{
		log_t::log("Starting Client");
		network::udp_connection conn;
		network::p2p_ps_connector_t p2p_con;
		p2p_con.stun_server_address1 = address;
		p2p_con.stun_server_address2 = address;
		//p2p_con.stun_server_address2.port++;

		p2p_con.transaction_id_ = 1337;
		network::connect_result_e result = conn.connect(p2p_con);
		
		if (result == network::connection_successful)
		{
			printf("1");
			std::thread heartbeater(thread_hb, std::ref(conn));
			
			while(!quit)
			{
				char input[254];
				conn.receive((uint8_t*)input,254);
			
				log_t::log("%s",input);
			}

			heartbeater.join();
		}else{
			printf("0");
		}
	}

	static void thread_hb(network::udp_connection & conn)
	{
		const sockaddr_in & myaddress = conn.get_socket().get_own_address();
		char msg[254];
		int len;
		len = snprintf(msg ,254,"%s | Hi!",inet_ntoa(myaddress.sin_addr));


		while(!quit)
		{
			sleep(3);
			conn.send((const uint8_t*)msg,len + 1);
		}
	}
};


int main(int argc, char* argv[])
{
	if (argc < 3) {
		usage();
		return 1;
	}

	network::address_t address;
	address.ip = address.parse_ip(argv[1]);
	if (address.ip == 0) {
		usage();
		return 1;
	}

	address.port = std::stoi(argv[2]);
	bool use_portscanning = false;

	if(argc == 5)
	{
		uint32_t stat_runs = std::stoi(argv[3]);
		use_portscanning = std::stoi(argv[4]);
		
	

		log_t::log("Starting Client in statistic mode");
		
		if(stat_runs <= 0) 
		{
			usage();
			return 2;
		}

		if(!use_portscanning)
		{
			statistical_runner runner;
			runner.run(address, stat_runs);
		}
		else
		{
			statistical_runner_ps runner;
			runner.run(address, stat_runs);
		}

		

	}
	else
	{
		if(argc == 4)
		{
			if(address.port <= 0)
			{
				usage();
				return 3;
			}

			use_portscanning = std::stoi(argv[3]);
			
			if(!use_portscanning)
			{
				normal_runner runner;
				runner.run(address);
			}
			else
			{
				normal_runner_ps runner;
				runner.run(address);
			}
		}else{
			usage();
			return 4;
		} 
	}

	return 0;
}

