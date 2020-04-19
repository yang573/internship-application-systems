#include <stdio.h>
#include <unistd.h>

static void print_help(void)
{
	fprintf(stdout, "Usage: yapu ADDRESS\n");
	//fprintf(stdout, "For a complete list of options, try \"yapu --help\" \n");
}

int main(int argc, char *argv[])
{
	char *address;

	// 1. Parse args
	if (argc < 2)
	{
		print_help();
		return 1;
	}

	address = argv[1];

	fprintf(stdout, "Address: %s\n", address);

	// 2. Fork process

	// 3. Setup signal handlers

	// 4. Enter loop

	// 5. Reap child (instead of loop?)

	// 6. Print out loop
}

// SIGINT handler
// Stops pinging

// SIGUSR1 handler?

