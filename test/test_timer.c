#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ltimer.h"

#define BUCKET_SIZE		3
#define BUCKET_CPUID	3

static void print_timer_info(void *data)
{
	struct timespec tms;
	curtime(&tms);
	printf("%s timeout at %ld.%ld\n", (const char *)data, tms.tv_sec, tms.tv_nsec);
}

int main()
{
	struct timespec tick = {0, 100};
	TimerBucketID_t bktid = create_timer_bucket("testbucket", BUCKET_SIZE, BUCKET_CPUID, tick);
	if (-1 == bktid) {
		exit(EXIT_FAILURE);
	}

	struct timespec t1 = {1, 0};
	TimerID_t timer_1 = add_timer(bktid, t1, print_timer_info, "Timer No.1 ", 1);

	struct timespec t2 = {2, 0};
	TimerID_t timer_2 = add_timer(bktid, t2, print_timer_info, "Timer No.2 ", 1);

	struct timespec t3 = {3, 0};
	TimerID_t timer_3 = add_timer(bktid, t3, print_timer_info, "Timer No.3 ", 1);

	struct timespec t4 = {4, 0};
	TimerID_t timer_4 = add_timer(bktid, t4, print_timer_info, "Timer No.4 ", 1);

	while (1) {
		pause();
	}

	del_timer(timer_1);
	del_timer(timer_2);
	del_timer(timer_3);
	del_timer(timer_4);
	destroy_timer_bucket(bktid);

	return 0;
}
