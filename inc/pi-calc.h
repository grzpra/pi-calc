#ifndef __PI_CALC_H
#define __PI_CALC_H

#define VERSION		"0.02"

struct thread_args {
	unsigned long int start;
	unsigned long int end;
	mpf_t partialsum;
};

#endif /* __PI_CALC_H */
