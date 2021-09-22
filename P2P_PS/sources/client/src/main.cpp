#include <common/log.h>
#include <common/event.h>

#include "connection/port_manager.h"
#include "connection/udp_connection.h"
#include "connection/p2p_connector.h"
#include "connection/p2p_ps_connector.h"

#include <thread>
#include <atomic>
#include <vector>
#include <condition_variable>




loglevel_e loglevel = loglevel_e::DEBUG;
bool quit = false;

network::port_manager_t portman = network::port_manager_t(75);

void usage()
{
	printf("Innvalid Arguments\n");
	printf("p2p_client <mode nr/snr> <target_ip> <target_port> <statistic_run_count?>\n");
	printf("p2p_client nr 10.10.10.10 3333\n");
	printf("p2p_client snr 10.10.10.10 3333 5000\n");
	printf("p2p_client <mode ps/sps> <target_ip1> <target_port1> <target_ip2> <target_port2> <is_sym_nat> <simulated_port_allocs_per_second> <statistic_run_count?>\n");
	printf("p2p_client ps 10.10.10.10 3333 10.10.10.11 3333 1 0\n");
	printf("p2p_client sps 10.10.10.10 3333 10.10.10.11 3333 1 50 5000\n");
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
		uint32_t processor_count =  16;

		//loglevel = ERROR;

		if(processor_count == 0) 
		{
			log_t::error("Could not get processor_count!");
			processor_count = 20;
		}
		
		for (uint32_t i = 0; i < processor_count; i++)
		{
			thread_list.push_back(std::thread(&statistical_runner::test_thread_main,this));
		}

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
	}

	network::address_t address_;
	uint32_t run_count = 0;

	std::atomic<uint32_t> successfull_runs;
	std::atomic<uint32_t> failed_runs;
	std::atomic<uint32_t> programm_mistakes;


	std::atomic<uint32_t> current_transaction_id;
	std::vector<std::thread> thread_list;
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

	void run(const network::address_t& address1, const network::address_t& address2, bool is_sym_nat, uint32_t runs,  uint16_t sim_port_allocs_per_second = 0)
	{	
		
		address1_ = address1;
		address2_ = address2;
		run_count = runs;
		is_sym_nat_ = is_sym_nat;
		log_t::log("Starting Statistical Client on %u runs", runs);
		uint32_t processor_count =  16;

		if(sim_port_allocs_per_second)
		{
			portman.set_simulated_allocs_per_second(sim_port_allocs_per_second);
			portman.set_reserved_port_range_per_alloc(sim_port_allocs_per_second * 5 + 50);
		}
	
		
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
			p2p_con.stun_server_address1 = address1_;
			p2p_con.stun_server_address2 = address2_;
			p2p_con.is_sym_nat = is_sym_nat_;
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

	}

	network::address_t address1_;
	network::address_t address2_;
	uint32_t run_count = 0;
	bool is_sym_nat_;

	std::atomic<uint32_t> successfull_runs;
	std::atomic<uint32_t> failed_runs;
	std::atomic<uint32_t> programm_mistakes;

	std::atomic<uint32_t> current_transaction_id;
	std::vector<std::thread> thread_list;

};

struct normal_runner_ps
{
	void run(network::address_t& address1,network::address_t& address2,bool is_sym_nat, uint16_t sim_port_allocs_per_second = 0)
	{
		log_t::log("Starting Client");
		network::udp_connection conn;
		network::p2p_ps_connector_t p2p_con;
		p2p_con.stun_server_address1 = address1;
		p2p_con.stun_server_address2 = address2;
		p2p_con.is_sym_nat = is_sym_nat;
		//p2p_con.stun_server_address2.port++;

		if(sim_port_allocs_per_second)
		{
			portman.set_simulated_allocs_per_second(sim_port_allocs_per_second);
			portman.set_reserved_port_range_per_alloc(sim_port_allocs_per_second * 5 + 50);
		}

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
	if (argc < 4) {
		usage();
		return 1;
	}

	if(strncmp("nr", argv[1], 2) == 0)
	{
		network::address_t address;
		address.ip = address.parse_ip(argv[2]);
		if (address.ip == 0) {
			usage();
			return 1;
		}
		address.port = std::stoi(argv[3]);

		normal_runner runner;
		runner.run(address);

	}
	else if (strncmp("snr", argv[1], 3) == 0)
	{
		network::address_t address;
		address.ip = address.parse_ip(argv[2]);
		if (address.ip == 0) {
			usage();
			return 1;
		}
		address.port = std::stoi(argv[3]);

		uint32_t stat_runs = std::stoi(argv[4]);
		statistical_runner runner;
		runner.run(address, stat_runs);
	}
	else if (strncmp("ps", argv[1], 2) == 0)
	{
		network::address_t address1;
		network::address_t address2;
		address1.ip = address1.parse_ip(argv[2]);
		if (address1.ip == 0) {
			usage();
			return 1;
		}
		address1.port = std::stoi(argv[3]);

		address2.ip = address2.parse_ip(argv[4]);
		if (address2.ip == 0) {
			usage();
			return 1;
		}
		address2.port = std::stoi(argv[5]);

		bool is_sym  = std::stoi(argv[6]);
		uint32_t sim_allocs = std::stoi(argv[7]);

		normal_runner_ps runner;
		runner.run(address1, address2, is_sym, sim_allocs);
		
	}
	else if (strncmp("sps", argv[1], 3) == 0)
	{
		network::address_t address1;
		network::address_t address2;

		address1.ip = address1.parse_ip(argv[2]);
		if (address1.ip == 0) {
			usage();
			return 1;
		}
		address1.port = std::stoi(argv[3]);

		address2.ip = address2.parse_ip(argv[4]);
		if (address2.ip == 0) {
			usage();
			return 1;
		}
		address2.port = std::stoi(argv[5]);
		
		bool is_sym  = std::stoi(argv[6]);
		uint32_t stat_runs = std::stoi(argv[7]);
		uint32_t sim_allocs = std::stoi(argv[8]);

		statistical_runner_ps runner;
		runner.run(address1, address2, is_sym, stat_runs, sim_allocs);
		
	}
	else
	{
		usage();
		return 4;
	}
	
	return 0;
}

