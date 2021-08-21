#include <common/log.h>
#include <common/event.h>

#include "connection/udp_connection.h"
#include <thread>
#include <stdatomic.h>
#include <vector>
#include <condition_variable>



loglevel_e loglevel = loglevel_e::DEBUG;
bool quit = false;

void usage()
{
	printf("p2p_client <target_ip> <target_port> <statistic_run_count>\n");
	printf("p2p_client 10.10.10.10 3333 100\n");
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
		run_count = runs;
		log_t::log("Starting Statistical Client on %u", runs);
		uint32_t processor_count = 1// std::thread::hardware_concurrency();

		if(processor_count == 0) 
		{
			log_t::error("Could not get processor_count!");
			processor_count = 6;
		}

		for (uint32_t i = 0; i < processor_count; i++)
		{
			thread_list.push_back(std::thread(test_thread_main,this));
		}
		
		quit_.wait();
		printf("\n\n\n");
		printf("------- STAT RUN COMPLETE -------\n");
		printf("Mode      : Basic Holepunching\n");
		printf("Runs      : %40u\n" , runs);
		printf("Succeeded : %40u\n" , successfull_runs);
		printf("Failed    : %40u\n" , failed_runs);
		printf("Errored	  : %40u\n" , programm_mistakes);
		printf("---------------------------------" , programm_mistakes);
		printf("\%suc     : %.3f                \n" , (runs-programm_mistakes)/successfull_runs);


		for (auto it = thread_list.begin(); it != thread_list.end(); ++it)
		{
			it->join();
		};

		return;
	}



	void test_thread_main()
	{

		for (uint32_t i = current_transaction_id++; i < run_count; i = current_transaction_id++)
		{
			network::udp_connection_t conn;
			network::connect_result_e result = conn.p2p_connect(i, address);
			
			switch (result)
			{
			case network::connection_successful:
				log_t::statistic("Run %40u succeeded",i);
				successfull_runs ++;
				break;
			case network::connection_failed_no_server_match_in_time:
				log_t::statistic("Run %40u errored - test clients mismatching",i);
				programm_mistakes ++;
				break;
			case network::connection_error:
				log_t::statistic("Run %40u errored - internal error",i);
				programm_mistakes ++;
				break;
			case network::connection_failed:
				log_t::statistic("Run %40u failed",i);
				failed_runs++;
				break;
			default:
				break;
			}

		}

		quit_.signal();
	}

	network::address_t& address;
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
		network::udp_connection_t conn;
		network::connect_result_e result = conn.p2p_connect(1337, address);
		
		if (result == network::connection_successful)
		{
			printf("1");
			std::thread heartbeater(thread_hb, std::ref(conn));
			
			while(!quit)
			{
				char input[254];
				conn.wait_till_receive((uint8_t*)input,254);
			
				log_t::log("%s",input);
			}

			heartbeater.join();
		}else{
			printf("0");
		}
	}

	static void thread_hb(network::udp_connection_t & conn)
	{
		const sockaddr_in & myaddress = conn.get_own_address();
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
	if (argc != 3) {
		usage();
		return 1;
	}
	network::address_t address;
	address.ip = address.parse_ip(argv[1]);
	if (address.ip == 0) {
		usage();
	}

	address.port = std::stoi(argv[2]);

	if(argc == 4)
	{
		log_t::log("Starting Client in statistic mode");
		uint32_t stat_runs = std::stoi(argv[3]);
		if(stat_runs <= 0) 
		{
			usage();
			return 2;
		}

		statistical_runner runner;
		runner.run(address, stat_runs);

	}
	else
	{
		if(address.port <= 0)
		{
			usage();
			return 3;
		}

		normal_runner runner;
		runner.run(address);
	}

	



	return 0;

}

