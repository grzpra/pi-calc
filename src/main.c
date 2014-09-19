#include <errno.h>
#include <getopt.h>
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pi-calc.h"

#define DEFAULT_DIGITS 1000

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
}

int chudnovsky(int digits)
{
	unsigned long int k, threek, iter, precision;
	mpf_t rtf, rbf, ltf, r, sum, result;
	mpz_t a, b, c, d, e, rt, rb;

	/* Calculate and set precision */
	precision = (digits * BPD) + 1;
	mpf_set_default_prec(precision);

	/* Calculate number of iterations */
	iter = digits/DPI;

	/* Init all objects */
	mpf_inits(rtf, rbf, ltf, r, sum, result, NULL);
	mpz_inits(a, b, c, d, e, rt, rb, NULL);
	mpf_set_ui(sum, 0);

	/* Prepare the constant from the left side of the equation */
	mpf_sqrt_ui(ltf, LTFCON1);
	mpf_mul_ui(ltf, ltf, LTFCON2);

	printf("Main loop starting....\n");
	printf("%d digits | %lu iterations \n", digits, iter);

	/* Main loop, can be parrelized */
	for (k = 0; k < iter; k++) {
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
		/* add result to the sum */
		mpf_add(sum, sum, r);
	}

	/* Invert sum */
	mpf_ui_div(sum, 1, sum);
	mpf_mul(result, sum, ltf);

	mpf_clears(rtf, rbf, ltf, r, sum, result, NULL);
	mpz_clears(a, b, c, d, e, rt, rb, NULL);

	return 0;
}

int main(int argc, char *argv[])
{
	int nsec, res, opt, digits = DEFAULT_DIGITS;
	double sec;
	double cpu_time, cpu_start, cpu_end;
	struct timespec start, end;

	const char* short_opts = "hd:";
	const struct option long_opts[] = {
		{ "help",	0, NULL, 'h' },
		{ "digits",	1, NULL, 'd' },
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
	res = chudnovsky(digits);
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
