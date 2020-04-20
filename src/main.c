#include <stdio.h>
#include <unistd.h>

#include <stdlib.h>
#include <signal.h>

#include "ping.h"

void sigint_handler(int signum);

static void print_help(void)
{
	fprintf(stdout, "Usage: yapu ADDRESS\n");
	//fprintf(stdout, "For a complete list of options, try \"yapu --help\" \n");
}

int main(int argc, char *argv[])
{
	char *address;
	int ret;

	// 1. Parse args
	if (argc < 2)
	{
		print_help();
		return 1;
	}

	address = argv[1];

	fprintf(stdout, "Address: %s\n", address);

	struct sigaction action, old_action;
	action.sa_handler = sigint_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_RESTART;
	sigaction(SIGINT, &action, &old_action);

	ret = send_ping(address);
	fprintf(stdout, "Ping ret: %d\n", ret);

	//for(;;) ;


	// 2. Fork process

	// 3. Setup signal handlers

	// 4. Enter loop

	// 5. Reap child (instead of loop?)

	// 6. Print out loop

	return 0;
}

// SIGINT handler
// Stops pinging
void sigint_handler(int signum)
{
	fprintf(stderr, "Sigint\n");
	exit(1);
}

// SIGUSR1 handler?

