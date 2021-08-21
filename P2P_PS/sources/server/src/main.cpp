#include <common/log.h>
#include "udp_p2p_server.h"
#include <csignal>

loglevel_e loglevel = loglevel_e::DEBUG;
bool sig_close = false;

void signalHandler(int signum) {

	log_t::log("interrupt %s received", signum);

	sig_close = true;

	exit(signum);
}


int main(int argc, char* argv[])
{
	log_t::log("Starting Server");
	signal(SIGINT, signalHandler);
	network::udp_server_t server;

	server.start();

}