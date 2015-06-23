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
#define LAST_DIGITS_PRINT 50

/* Digits per iteration (where is it from?) */
#define DPI 14.1816474627254776555
/* Bits per digit (log2(10)) */
#define BPD 3.32192809488736234787

#define BCONST1 545140134
#define BCONST2 13591409
#define DCONST1 3
#define ECONST1 640320
#define LTFCON1	10005
#define LTFCON2	426880

#define print_err(str, val) fprintf(stderr, "%s (%d): %s.\n", str, val, strerror(-val))

void print_usage(void)
{
	printf("Supported options:\n");
	printf(" -h --help	prints this message and exits\n");
	printf(" -d --digits	number of digits for Pi calculations\n");
	printf("		(default is %d)\n", DEFAULT_DIGITS);
	printf(" -t --threads	number of threads to run\n");
	printf("		(by default # of threads = # of logical cores)\n");
}

/* Gets the number of available CPUs based on available affinities */
/* TODO: OS specific? Might be better to use different method */
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
	unsigned long int k, threek, total_iterations;
	mpz_t a, b, c, d, e, rt, rb;
	mpf_t rtf, rbf, r;
	struct thread_args *args = (struct thread_args *)arguments;

	total_iterations = args->iter;

	/* Init libgmp variables */
	mpz_inits(a, b, c, d, e, rt, rb, NULL);
	mpf_inits(rtf, rbf, r, NULL);

	/* Main loop of a thread */
	while (1) {
		/* Check which k needs calculation */
		pthread_mutex_lock(&args->start_mutex);
		k = args->k;
		args->k++;
		pthread_mutex_unlock(&args->start_mutex);
		/* If all ks are done, say return */
		if (k > total_iterations) break;
		/* 3k */
		threek = k * 3;
		/* (6k!) */
		mpz_fac_ui(a, threek << 1);
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
		if ((threek & 1) == 1) mpz_neg(e, e);
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
		/* add result to the sum */

		pthread_mutex_lock(&args->sum_mutex);
		mpf_add(args->sum, args->sum, r);
		pthread_mutex_unlock(&args->sum_mutex);
	}

	/* Deinit variables, result is passed via args->sum */
	mpz_clears(a, b, c, d, e, rt, rb, NULL);
	mpf_clears(rtf, rbf, r, NULL);
}

int chudnovsky(int digits, int threads)
{
	int res = 0;
	unsigned long int i, iter, precision, rest, per_cpu, k;
	mpf_t ltf, sum, result;
	pthread_t *pthreads;
	struct thread_args targs;
        mp_exp_t exponent;
        char *pi;

	/* If threads is not specified, check how many CPUs are avail */
	if (threads == 0) {
		threads = get_cpu_count();
	}

	pthreads = malloc(threads * sizeof(pthread_t));
	if (pthreads == NULL) {
		res = -ENOMEM;
		goto chudnovsky_exit;
	}

	/* Calculate and set precision */
	precision = (digits * BPD) + 1;
	mpf_set_default_prec(precision);

	/* Calculate number of iterations */
	iter = digits/DPI + 1;

	/* Init all objects */
	mpf_inits(ltf, sum, result, targs.sum, NULL);
	mpf_set_ui(sum, 0);
	mpf_set_ui(targs.sum, 0);

	/* Set pthread specific stuff */
	targs.k = 0;
	targs.iter = iter;
	pthread_mutex_init(&targs.start_mutex, NULL);
	pthread_mutex_init(&targs.sum_mutex, NULL);

	/* Prepare the constant from the left side of the equation */
	mpf_sqrt_ui(ltf, LTFCON1);
	mpf_mul_ui(ltf, ltf, LTFCON2);

	printf("Starting summing, using:\n"
		"%d digits - %lu iterations - %d threads\n",
		digits, iter, threads);

	for (i = 0; i < threads; i++) {
		pthread_create(&pthreads[i], NULL, &chudnovsky_chunk, (void *) &targs);
	}

	/* Wait for threads to finish and take their sums */
	for (i = 0; i < threads; i++) {
		pthread_join(pthreads[i], NULL);
	}

	printf("Starting final steps\n");

	/* Invert sum */
	mpf_ui_div(sum, 1, targs.sum);
	mpf_mul(result, sum, ltf);

	pi = mpf_get_str(NULL, &exponent, 10, digits + 1, result);

	if (strlen(pi) < LAST_DIGITS_PRINT + 1) {
		printf("Calculated PI:\n");
		printf("\t%.*s.%s\n", (int)exponent, pi, pi + exponent);
	} else {
		printf("Last digits of Pi are:\n");
		printf("\t%s\n", pi+(digits-(LAST_DIGITS_PRINT-1)));
	}

	free(pi);

	mpf_clears(ltf, sum, result, NULL);
	pthread_mutex_destroy(&targs.start_mutex);
	pthread_mutex_destroy(&targs.sum_mutex);

	/* TODO: add verification here! */

chudnovsky_free_pthreads:
	free(pthreads);
chudnovsky_exit:
	return res;
}

int main(int argc, char *argv[])
{
	int nsec, res = 0, opt, digits = DEFAULT_DIGITS, threads = 0;
	double sec;
	double cpu_time, cpu_start, cpu_end;
	struct timespec start, end;

	const char* short_opts = "hd:t:";
	const struct option long_opts[] = {
		{ "help",	0, NULL, 'h' },
		{ "digits",	1, NULL, 'd' },
		{ "threads",	1, NULL, 't' },
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
		case 't':
			threads = atoi(optarg);
			if (threads <= 0) {
				res = -EINVAL;
				print_err("Wrong threads count", res);
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
	res = chudnovsky(digits, threads);
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
