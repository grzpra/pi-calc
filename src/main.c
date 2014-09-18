#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "pi-calc.h"

#define print_err(str,val)	printf("%s (%d): %s.\n", str, val, strerror(-val))

int execute(void)
{
	sleep(1);
	return 0;
}

void calculate_td(struct timespec start, struct timespec end, double *sec, int *nsec)
{
	*sec = difftime(end.tv_sec, start.tv_sec);
	if (end.tv_nsec >= start.tv_nsec) {
		*nsec = end.tv_nsec - start.tv_nsec;
	} else {
		*nsec = start.tv_nsec - end.tv_nsec;
		*sec--;
	}
}

int main(int argc, char *argv[])
{
	int nsec, res;
	double sec;
	struct timespec start, end;

	printf("pi-calc version %s started.\n", VERSION)

	res = clock_gettime(CLOCK_MONOTONIC, &start);
	if (res < 0) {
		res = -errno;
		print_err("Cannot obtain start time", res);
		return res;
	}

	res = execute();
	if (res < 0) {
		print_err("Error during execution", res);
		return res;
	}

	res = clock_gettime(CLOCK_MONOTONIC, &end);
	if (res < 0) {
		res = -errno;
		print_err("Cannot obtain stop time", res);
		return res;
	}

	calculate_td(start, end, &sec, &nsec);

	printf("Run time: %.0f.%.9d s\n", sec, nsec);

	return 0;
}
