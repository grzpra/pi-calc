#define _GNU_SOURCE

#include <errno.h>
#include <getopt.h>
#include <gmp.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pi-calc.h"

#define DEFAULT_DIGITS 1000
#define MAX_CPU_NUMBER 32

/* Digits per iteration (where is it from?) */
#define DPI 14.1816474627254776555
/* Bits per digit (log2(10)) */
#define BPD 3.32192809488736234789

#define BCONST1 545140134
#define BCONST2 13591409
#define DCONST1 3
#define ECONST1 -640320
#define LTFCON1	10005
#define LTFCON2	426880

#define print_err(str, val) fprintf(stderr, "%s (%d): %s.\n", str, val, strerror(-val))

void print_usage(void)
{
	printf("Supported options:\n");
	printf(" -h --help	prints this message and exits\n");
	printf(" -d --digits	number of digits for Pi calculations\n");
	printf("		(default is %d)\n", DEFAULT_DIGITS);
	printf(" -c --cpus	number of threads to run\n");
	printf("		(by default # of threads = # of logical cores)\n");
}

int get_cpu_count(void)
{
	cpu_set_t cs;

	CPU_ZERO(&cs);
	if (sched_getaffinity(0, sizeof(cs), &cs))
		return 1;

	return CPU_COUNT(&cs);
}

void *chudnovsky_chunk(void *arguments)
{
	unsigned long int k, threek;
	mpz_t a, b, c, d, e, rt, rb;
	mpf_t rtf, rbf, r;

	struct thread_args *args = (struct thread_args *)arguments;

	mpz_inits(a, b, c, d, e, rt, rb, NULL);
	mpf_inits(rtf, rbf, r, NULL);
	mpf_set_ui(args->partialsum, 0);

	/* Main loop, can be parrelized */
	for (k = args->start; k < args->end; k++) {
		/* 3k */
		threek = k * 3;
		/* (6k!) */
		mpz_fac_ui(a, threek << 2);
		/* 545140134k */
		mpz_set_ui(b, BCONST1);
		mpz_mul_ui(b, b, k);
		/* 13591409 + 545140134k */
		mpz_add_ui(b, b, BCONST2);
		/* (3k)! */
		mpz_fac_ui(c, threek);
		/* (k!)^3 */
		mpz_fac_ui(d, k);
		mpz_pow_ui(d, d, DCONST1);
		/* (-640320)^3k */
		mpz_set_ui(e, ECONST1);
		mpz_pow_ui(e, e, threek);
		/* Get everything together */
		/* (6k)! (13591409 + 545140134k) */
		mpz_mul(rt, a, b);
		/* (3k)! (k!)^3 */
		mpz_mul(rb, c, d);
		mpz_mul(rb, rb, e);
		/* Switch to floats */
		mpf_set_z(rtf, rt);
		mpf_set_z(rbf, rb);
		/* divide top/bottom */
		mpf_div(r, rtf, rbf);
		/* add result to the partialsum */
		mpf_add(args->partialsum, args->partialsum, r);
	}

	mpz_clears(a, b, c, d, e, rt, rb, NULL);
	mpf_clears(rtf, rbf, r, NULL);
}

int chudnovsky(int digits, int cpus)
{
	unsigned long int i, iter, precision, rest, per_cpu;
	mpf_t ltf, sum, result;
	pthread_t threads[MAX_CPU_NUMBER];
	struct thread_args targs[MAX_CPU_NUMBER];

#ifdef DEBUG_PRINT
	mp_exp_t exp;
	char *debug_out;
#endif /* DEBUG_PRINT */

	if (cpus == 0) {
		cpus = get_cpu_count();

		if (cpus > MAX_CPU_NUMBER) {
			cpus = MAX_CPU_NUMBER;
		}
	}

	/* Calculate and set precision */
	precision = (digits * BPD) + 1;
	mpf_set_default_prec(precision);

	/* Calculate number of iterations */
	iter = digits/DPI;

	/* Init all objects */
	mpf_inits(ltf, sum, result, NULL);
	mpf_set_ui(sum, 0);

	/* Prepare the constant from the left side of the equation */
	mpf_sqrt_ui(ltf, LTFCON1);
	mpf_mul_ui(ltf, ltf, LTFCON2);

	printf("Main loop starting....\n");
	printf("%d digits | %lu iterations \n", digits, iter);

	rest = iter % cpus;
	per_cpu = iter / cpus;

	for (i = 0; i < cpus; i++) {
		mpf_inits(targs[i].partialsum);
	}

	for (i = 0; i < cpus; i++) {
		targs[i].start = per_cpu * i;
		if (i != 0)
			targs[i].start++;

		targs[i].end = per_cpu * (i + 1);
		if ((i + 1) == cpus)
			targs[i].end += rest;

		pthread_create(&threads[i], NULL, &chudnovsky_chunk, (void *) &targs[i]);
	}

	for (i = 0; i < cpus; i++) {
		pthread_join(threads[i], NULL);
		mpf_add(sum, sum, targs[i].partialsum);
	}

	/* Invert sum */
	mpf_ui_div(sum, 1, sum);
	mpf_mul(result, sum, ltf);

#ifdef DEBUG_PRINT
	debug_out = mpf_get_str(NULL, &exp, 10, 0, result);
	printf("%.*s.%s\n", (int)exp, debug_out, debug_out + exp);
#endif /* DEBUG_PRINT */

	mpf_clears(ltf, sum, result, NULL);

	return 0;
}

int main(int argc, char *argv[])
{
	int nsec, res, opt, digits = DEFAULT_DIGITS, cpus = 0;
	double sec;
	double cpu_time, cpu_start, cpu_end;
	struct timespec start, end;

	const char* short_opts = "hd:c:";
	const struct option long_opts[] = {
		{ "help",	0, NULL, 'h' },
		{ "digits",	1, NULL, 'd' },
		{ "cpus",	1, NULL, 'c' },
	};

	printf("pi-calc version %s\n", VERSION);

	do {
		opt = getopt_long(argc, argv, short_opts, long_opts, NULL);
		switch(opt)
		{
		case 'h':
			print_usage();
			return 0;
		case 'd':
			digits = atoi(optarg);
			if (digits <= 0) {
				res = -EINVAL;
				print_err("Wrong digits value", res);
				return res;
			}
			break;
		case 'c':
			cpus = atoi(optarg);
			if (cpus <= 0 || cpus > MAX_CPU_NUMBER) {
				res = -EINVAL;
				print_err("Wrong CPU count", res);
				return res;
			}
			break;
		case -1:
			break;
		default:
			print_usage();
			res = -EINVAL;
			return res;
		}
	} while (opt != -1);

	/* Get CPU and normal time */
	cpu_start = clock();
	res = clock_gettime(CLOCK_MONOTONIC, &start);
	if (res < 0) {
		res = -errno;
		print_err("Cannot obtain start time", res);
		return res;
	}

	/* Run the Chudnovsky algorithm */
	res = chudnovsky(digits, cpus);
	if (res < 0) {
		print_err("Error during execution", res);
		return res;
	}

	/* Get the end time */
	cpu_end = clock();
	res = clock_gettime(CLOCK_MONOTONIC, &end);
	if (res < 0) {
		res = -errno;
		print_err("Cannot obtain stop time", res);
		return res;
	}

	sec = difftime(end.tv_sec, start.tv_sec);
	if (end.tv_nsec >= start.tv_nsec) {
		nsec = end.tv_nsec - start.tv_nsec;
	} else {
		nsec = start.tv_nsec - end.tv_nsec;
		sec--;
	}
	cpu_time = ((double)(cpu_end - cpu_start)) / CLOCKS_PER_SEC;

	printf("Run time: %.0f.%.9d s\n", sec, nsec);
	printf("CPU time: %.9f s\n", cpu_time);

	return 0;
}
