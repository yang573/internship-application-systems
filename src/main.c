#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <stdlib.h>
#include <signal.h>

#include "defs.h"
#include "ping.h"

void sigint_handler(int signum);

/* Prints out usage info
 */
static void print_help(void)
{
	fprintf(stdout, "Usage: yapu ADDRESS\n");
	//fprintf(stdout, "For a complete list of options, try \"yapu --help\" \n");
}

int main(int argc, char *argv[])
{
	pthread_t ping_thread, recv_thread;
	char *address;
	int socket_fd;
	int ret;

	// 1. Parse args
	if (argc != 2)
	{
		print_help();
		return 1;
	}

	address = argv[1];

	// 2. Setup SIGINT handler
	struct sigaction action, old_action;
	action.sa_handler = sigint_handler;
	ret = sigemptyset(&action.sa_mask);
	if (ret != 0)
	{
		perror("Error setting up signal handler");
	}
	action.sa_flags = SA_RESTART;
	ret = sigaction(SIGINT, &action, &old_action);
	if (ret != 0)
	{
		perror("Error setting up signal handler");
	}

	// 2. Get socket fd
	socket_fd = init_ping(address, DEFAULT_TTL);
	if (socket_fd < 0)
		return socket_fd;

	// 3. Create threads for sending/receiving packets
	ret = pthread_create(&ping_thread, NULL, ping_routine, (void*)(intptr_t)socket_fd);
	if (ret != 0)
	{
		fprintf(stderr, "Error creating threads. Aborting...\n");
		return ERR_FATAL;
	}

	ret = pthread_create(&recv_thread, NULL, recv_routine, (void*)(intptr_t)socket_fd);
	if (ret != 0)
	{
		fprintf(stderr, "Error creating threads. Aborting...\n");
		return ERR_FATAL;
	}

	// 4. Wait for user to cancel
	pthread_mutex_lock(&ping_loop_lock);
	if (ping_loop == 1)
		pthread_cond_wait(&ping_loop_cond, &ping_loop_lock);
	pthread_mutex_unlock(&ping_loop_lock);

	// 5. Join threads
	pthread_join(ping_thread, NULL);
	pthread_join(recv_thread, NULL);

	// 6. Print out results
	ping_results();

	// 7. Clean up
	cleanup_ping(socket_fd);

	return 0;
}

/* SIGINT handler
 * Stops the ping loop
 */
void sigint_handler(int signum)
{
	fprintf(stderr, "Sigint\n");
	pthread_mutex_lock(&ping_loop_lock);
	ping_loop = 0;
	pthread_cond_signal(&ping_loop_cond);
	pthread_mutex_unlock(&ping_loop_lock);
}

