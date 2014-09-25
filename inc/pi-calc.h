#ifndef __PI_CALC_H
#define __PI_CALC_H

#define VERSION		"0.04"

struct thread_args {
	pthread_mutex_t start_mutex;
	pthread_mutex_t sum_mutex;
	unsigned long k;
	unsigned long iter;
	mpf_t sum;
};

#endif /* __PI_CALC_H */
