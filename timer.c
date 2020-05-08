#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#define _POSIX_C_SOURCE_ 199309
#include <time.h>

#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include "list_head.h"
#include "utils.h"

#include "timer.h"

#define MAX_EVENT_NUMBER	8

enum
{
	EN_TIMER_OPT_ADD,
	EN_TIMER_OPT_DEL,
	EN_TIMER_OPT_MOD,
};

typedef struct st_ltimer_opt
{
	int					opt;		//refer to EN_TIMER_OPT_XXX.
	int					pad;		//padding bytes.
	TimerID_t			id;			//timer id.
	struct timespec		period;		//period time.
	time_out_proc		func;		//callback func when timeout.
	void *				data;		//data for callback.
}ltimer_opt_t;

typedef struct st_ltimer
{
	struct list_head	entry;		//list head entry.

	void *				p_bkt;		//relate to timer bucket.

	struct timespec		period;		//period time.
	struct timespec		expire;		//expire time, use abs time.
	time_out_proc		func;		//callback func when timeout.
	void *				data;		//data for callback.
	
	int					type;		//cycle(1) or once(0) timer.
	int					pad;		//padding bytes.
}ltimer_t;

typedef struct st_timer_bucket
{
	struct list_head	active_list;	//active timer list head entry.
	struct list_head	recycle_list;	//dead timer list head entry.
	
	char*				mem_alloc_ptr;	//memory ptr for final release.
	char*				cur_alloc_ptr;	//memory ptr for next allocation.

	pthread_spinlock_t	splock;			//lock for recycle_list and cur_alloc_ptr.

	struct timespec		curtime;		//system time, update every tick.
	struct timespec		resolution;		//tick, resolution for timer bucket.
	
	pthread_t			thread_id;		//work thread id.

	char				name[16];		//bucket name, will name to work thread.
	int					pipefd[2];		//pipe for accept new io event fd.
	int					epoll_fd;		//poll fd.
	int					timerfd;		//bucket timer fd, by timerfd_create.
	
	int					size;			//bucket max size.
	int					count;			//current timer count in bucket.
	int					cpuid;			//core id to bind.
	int					trigger;		//control working thread.
}timer_bucket_t;

struct timespec g_sys_curtime;	//system real time, can be used by other module with efficiency.

static int add_epoll_event(int fd, int event, int epollfd)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = event;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

static inline int compare_timespec(struct timespec t1, struct timespec t2)
{
	if (t1.tv_sec != t2.tv_sec) {
		return (t1.tv_sec - t2.tv_sec);
	} else {
		return (t1.tv_nsec - t2.tv_nsec);
	}
}

static void update_curtime(struct timespec* cur, struct timespec* tick, uint64_t nex)
{
 	cur->tv_sec += (nex * tick->tv_sec);
	cur->tv_nsec += (nex * tick->tv_nsec);
	while (cur->tv_nsec > 1000000000) {
		cur->tv_sec += 1;
		cur->tv_nsec -= 1000000000;
	}
	
	__sync_lock_test_and_set(&g_sys_curtime.tv_sec, cur->tv_sec);
	__sync_lock_test_and_set(&g_sys_curtime.tv_nsec, cur->tv_nsec);
}

static inline int compare_timer_by_entry(list_head_t* ptr1, list_head_t* ptr2)
{
	ltimer_t* p_timer1 = list_entry(ptr1, ltimer_t, entry);
	ltimer_t* p_timer2 = list_entry(ptr2, ltimer_t, entry);
	return compare_timespec(p_timer1->expire, p_timer2->expire);
}

static void check_timers_in_bucket(timer_bucket_t *p_bkt)
{
	ltimer_t *pos, *n;
	list_for_each_entry_safe(pos, n, &p_bkt->active_list, entry) {
		if (compare_timespec(p_bkt->curtime, pos->expire) < 0) {
			return;
		}

		//already timeout, call proc func.
		pos->func(pos->data);
		
		//once timer, remove from active list and recycle.
		//cycle timer, remove from active list, setup time, insert into active list again. 
		if (0 == pos->type) {
			list_del(&pos->entry);
			list_add_tail(&pos->entry, &p_bkt->recycle_list);
		} else {
			list_del(&pos->entry);
			pos->expire.tv_sec = p_bkt->curtime.tv_sec + pos->period.tv_sec;
			pos->expire.tv_nsec = p_bkt->curtime.tv_nsec + pos->period.tv_nsec;
			list_insert_reverse(&pos->entry, &p_bkt->active_list, compare_timer_by_entry, 1);
		}
	}
}

static void proc_timer_opt_event(int pipefd, timer_bucket_t *p_bkt)
{
	int nread = 0;
	ltimer_opt_t optev;
	ltimer_t* p_timer = NULL;

	do {
		nread = read( pipefd, &optev, sizeof(ltimer_opt_t));
		if (-1 == nread || 0 == nread) {
			return;
		}
		
		if ( nread != sizeof(ltimer_opt_t) ) {
			printf("%s: read %d bytes, error info: %s!\n", __func__, nread, strerror(errno));
			break;
		}

		p_timer = (ltimer_t *)optev.id;
		switch (optev.opt)
		{
			case EN_TIMER_OPT_ADD: {
				p_timer->expire.tv_sec = p_bkt->curtime.tv_sec + p_timer->period.tv_sec;
				p_timer->expire.tv_nsec = p_bkt->curtime.tv_nsec + p_timer->period.tv_nsec;
				list_insert_reverse(&p_timer->entry, &p_bkt->active_list, compare_timer_by_entry, 1);
				__sync_add_and_fetch(&p_bkt->count, 1);
				break;
			};
			case EN_TIMER_OPT_DEL: {
				list_del(&p_timer->entry);
				list_add_tail(&p_timer->entry, &p_bkt->recycle_list);
				__sync_sub_and_fetch(&p_bkt->count, 1);
				break;
			};
			case EN_TIMER_OPT_MOD: {
				if (NULL != optev.func) {
					p_timer->func = optev.func;
				}
				if (NULL != optev.data) {
					p_timer->data = optev.data;
				}
				if (optev.period.tv_sec != 0 || optev.period.tv_nsec != 0) {
					list_del(&p_timer->entry);
					p_timer->expire.tv_sec = p_bkt->curtime.tv_sec + optev.period.tv_sec;
					p_timer->expire.tv_nsec = p_bkt->curtime.tv_nsec + optev.period.tv_nsec;
					list_insert_reverse(&p_timer->entry, &p_bkt->active_list, compare_timer_by_entry, 1);
				}
				break;
			};
			default: { break; }
		}
	}while(1);
}

static void* work_routine(void *arg)
{
	int retval;
	timer_bucket_t* p_info = (timer_bucket_t*)arg;

	//set core affinity.
	retval = set_thread_core_affinity(p_info->cpuid, p_info->thread_id);
	if (0 != retval) {
		printf("set_thread_core_affinity failed with error info: %s\n", strerror(errno));
		return NULL;
	}

	//set work thread name.
	retval = set_thread_name(p_info->name, p_info->thread_id);
	if (0 != retval) {
		printf("set_thread_name failed with error info: %s\n", strerror(errno));
		return NULL;
	}
	
	//add pipefd[0] into epoll event.
	retval = add_epoll_event(p_info->pipefd[0], (EPOLLIN | EPOLLET), p_info->epoll_fd);
	if (0 != retval) {
		printf("add_epoll_event failed with error info: %s\n", strerror(errno));
		return NULL;
	}
	
	//get realtime from system.
	retval = clock_gettime(CLOCK_REALTIME, &p_info->curtime);
	if (0 != retval) {
		printf("clock_gettime failed with error info: %s!\n", strerror(errno));
		return NULL;
	}
	printf("%s: p_info->curtime.tv_sec = %ld\n", __func__, p_info->curtime.tv_sec);
	printf("%s: p_info->curtime.tv_nsec = %ld\n", __func__, p_info->curtime.tv_nsec);
	
	//add update system time event into epollfd.
	struct itimerspec itmspec;
	itmspec.it_value = p_info->resolution;
	itmspec.it_interval = p_info->resolution;
	retval = timerfd_settime(p_info->timerfd, 0, &itmspec, NULL);
	if (0 != retval) {
		printf("timerfd_settime failed with error info: %s!\n", strerror(errno));
		return NULL;
	}
	retval = add_epoll_event(p_info->timerfd, (EPOLLIN | EPOLLET), p_info->epoll_fd);
	if (0 != retval) {
		printf("add_epoll_event failed with error info: %s\n", strerror(errno));
		return NULL;
	}
	
	//epoll event loop.
	int i, nread, nfds;
	uint64_t nexpired = 0;
    struct epoll_event events[MAX_EVENT_NUMBER];
	
	while (1 == p_info->trigger) {
		nfds = epoll_wait(p_info->epoll_fd, events, MAX_EVENT_NUMBER, -1);
		if (nfds <= 0) {
			continue;
		}

		for (i = 0; i < nfds; i++) {
			//tick event coming, update system time, check timers in bucket.
			if ( events[i].data.fd == p_info->timerfd && (events[i].events & EPOLLIN ) ) {
				nread = read(p_info->timerfd, &nexpired, sizeof(uint64_t));
				if ( sizeof(uint64_t) != nread ) {
					continue;
				}
				//printf("tick event coming [%ld]!\n", nexpired);
				update_curtime(&p_info->curtime, &p_info->resolution, nexpired);
				check_timers_in_bucket(p_info);
			}
			//pipe event coming, proc add/del/mod timer event.
			else if (events[i].data.fd == p_info->pipefd[0]  && (events[i].events & EPOLLIN ) ) {
				//printf("pipe event coming!\n");
				proc_timer_opt_event(p_info->pipefd[0], p_info);
			}
		}
	}

	return NULL;
}
//---------------------------------------------------------------------------------------------------------
TimerBucketID_t create_timer_bucket(const char *name, int size, int cpuid, struct timespec resolution)
{
	if ( size < 0 || cpuid < 0 
		|| resolution.tv_sec < 0 || resolution.tv_nsec < 0 
		|| (resolution.tv_sec == 0 && resolution.tv_nsec == 0) ) {
		printf("%s: Invalid parameter!\n", __func__);
		return -1;
	}

	int retval = 0;
	timer_bucket_t *p_bkt = (timer_bucket_t *)calloc(sizeof(timer_bucket_t), 1);
	if ( NULL == p_bkt ) {
		printf("%s: calloc failed!\n", __func__);
		return -1;
	}
	p_bkt->mem_alloc_ptr = (char *)calloc(sizeof(ltimer_t), size);
	if ( NULL == p_bkt->mem_alloc_ptr ) {
		printf("%s: calloc failed!\n", __func__);
		retval = -1;
		goto cleanup;
	}

	if ( NULL != name ) {
		snprintf(p_bkt->name, sizeof(p_bkt->name), "%s", name);
	}

	p_bkt->timerfd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
	if (p_bkt->timerfd < 0) {
		printf("%s: timerfd_create failed with error info: %s!\n", __func__, strerror(errno));
		retval = -1;
		goto cleanup;
	}

	retval = pipe2(p_bkt->pipefd, O_NONBLOCK);
	if (0 != retval) {
		printf("%s: pipe2 failed with error info: %s!\n", __func__, strerror(errno));
		goto cleanup;
	}

	p_bkt->epoll_fd = epoll_create(MAX_EVENT_NUMBER);
	if ( p_bkt->epoll_fd < 0 ) {
		printf("%s: epoll_create failed with error info: %s\n", __func__, strerror(errno));
		retval = -1;
		goto cleanup;
	}

	list_init(&p_bkt->active_list);
	list_init(&p_bkt->recycle_list);
	pthread_spin_init(&p_bkt->splock, 0);
	p_bkt->cur_alloc_ptr = p_bkt->mem_alloc_ptr;
	p_bkt->resolution = resolution;
	p_bkt->size = size;
	p_bkt->cpuid = cpuid;
	p_bkt->trigger = 1;
	p_bkt->count = 0;

	retval = pthread_create(&p_bkt->thread_id, NULL, work_routine, (void*)p_bkt);
	if (retval != 0) {
		printf("%s: pthread_create failed with error info: %s!\n", __func__, strerror(errno));
		retval = -1;
		goto cleanup;
	}

	//wait for working thread update curtime.
	while(0 == p_bkt->curtime.tv_sec) {
		usleep(10);
	}
	
	return (TimerBucketID_t)p_bkt;

cleanup:
	if (NULL != p_bkt->mem_alloc_ptr) {
		free(p_bkt->mem_alloc_ptr);
		p_bkt->mem_alloc_ptr = NULL;
	}
	if ( 0 != p_bkt->timerfd ) {
		close(p_bkt->timerfd); p_bkt->timerfd = 0;
	}
	if ( 0 != p_bkt->pipefd[0] ) {
		close(p_bkt->pipefd[0]); p_bkt->pipefd[0] = 0;
		close(p_bkt->pipefd[1]); p_bkt->pipefd[1] = 0;
	}
	if ( 0 != p_bkt->epoll_fd ) {
		close(p_bkt->epoll_fd); p_bkt->epoll_fd = 0;
	}
	if (NULL != p_bkt) {
		free(p_bkt); p_bkt = NULL;
	}
	return -1;
}

void destroy_timer_bucket(TimerBucketID_t bktid)
{
	timer_bucket_t *p_bkt = (timer_bucket_t *)bktid;
	
	p_bkt->trigger = 0;
	pthread_join(p_bkt->thread_id, NULL);
	pthread_spin_destroy(&p_bkt->splock);
	if (0 != p_bkt->timerfd) {
		close(p_bkt->timerfd); p_bkt->timerfd = 0;
	}
	if (0 != p_bkt->pipefd[0]) {
		close(p_bkt->pipefd[0]); p_bkt->pipefd[0] = 0;
		close(p_bkt->pipefd[1]); p_bkt->pipefd[1] = 0;
	}
	if (0 != p_bkt->epoll_fd) {
		close(p_bkt->epoll_fd); p_bkt->epoll_fd = 0;
	}
	if (NULL != p_bkt->mem_alloc_ptr) {
		free(p_bkt->mem_alloc_ptr); p_bkt->mem_alloc_ptr = NULL;
	}
	free(p_bkt);
}

TimerID_t add_timer(TimerBucketID_t bktid, struct timespec tm, time_out_proc func, void *data, int type)
{
	if (bktid == 0 
		|| tm.tv_sec < 0 || tm.tv_nsec < 0 || (tm.tv_sec == 0 && tm.tv_nsec == 0)
		|| NULL == func || NULL == data || (type != 0 && type != 1)) {
		printf("%s: Invalid parameter!\n", __func__);
		return -1;
	}

	int nret = 0;
	ltimer_opt_t optev;
	ltimer_t *p_timer = NULL;	
	timer_bucket_t *p_bkt = (timer_bucket_t *)bktid;

	//get new timer from recycle_list or resource pool.
	pthread_spin_lock(&p_bkt->splock);
	if ( !list_empty(&p_bkt->recycle_list) ) {
		p_timer = (ltimer_t *) list_entry(p_bkt->recycle_list.next, ltimer_t, entry);
		list_del(p_bkt->recycle_list.next);
	} else if ( (int)((p_bkt->cur_alloc_ptr - p_bkt->mem_alloc_ptr)/sizeof(ltimer_t)) < p_bkt->size ) {
		p_timer = (ltimer_t *)p_bkt->cur_alloc_ptr;
		p_bkt->cur_alloc_ptr += sizeof(ltimer_t);
	}
	pthread_spin_unlock(&p_bkt->splock);

	if (NULL == p_timer) {
		printf("%s: Timer bucket is full, can't add no more!\n", __func__);
		return -1;
	}

	//init timer.
	list_init(&p_timer->entry);
	p_timer->p_bkt = p_bkt;
	p_timer->period = tm;
	p_timer->func = func;
	p_timer->data = data;
	p_timer->type = type;

	//write add event to pipe.
	memset(&optev, 0, sizeof(ltimer_opt_t));
	optev.opt = EN_TIMER_OPT_ADD;
	optev.id = (TimerID_t)p_timer;
	
	//writing bytes less than PIPE_BUF is atomic operation is guaranteed by system.
	nret = write(p_bkt->pipefd[1], &optev, sizeof(ltimer_opt_t));
	if (nret != sizeof(ltimer_opt_t)) {
		printf("%s: write pipe error!\n", __func__);
		goto cleanup;
	}

	return (TimerID_t)p_timer;

cleanup:
	pthread_spin_lock(&p_bkt->splock);
	list_add_head(&p_timer->entry, &p_bkt->recycle_list);
	pthread_spin_unlock(&p_bkt->splock);
	return -1;
}

int mod_timer(TimerID_t timerid, struct timespec tm, time_out_proc func, void *data)
{
	ltimer_t *p_timer = (ltimer_t *)timerid;
	if ( NULL == p_timer || NULL == p_timer->p_bkt ) {
		printf("%s: Invalid timer id!\n", __func__);
		return -1;
	}

	int nret = 0;
	ltimer_opt_t optev;
	timer_bucket_t *p_bkt = (timer_bucket_t *)p_timer->p_bkt;
	
	memset(&optev, 0, sizeof(ltimer_opt_t));
	optev.opt = EN_TIMER_OPT_MOD;
	optev.id = timerid;
	if (tm.tv_sec >= 0 && tm.tv_nsec >= 0 && !(tm.tv_sec == 0 && tm.tv_nsec == 0)) {
		optev.period = tm;
	}
	if (NULL != func) {
		optev.func = func;
	}
	if(NULL != data) {
		optev.data = data;
	}
	
	nret = write(p_bkt->pipefd[1], &optev, sizeof(ltimer_opt_t));
	if ( nret != sizeof(ltimer_opt_t) ) {
		printf("%s: write pipe error!\n", __func__);
		return -1;
	}
	return 0;
}

int del_timer(TimerID_t timerid)
{
	ltimer_t *p_timer = (ltimer_t *)timerid;
	if (NULL == p_timer || NULL == p_timer->p_bkt) {
		printf("%s: Invalid timer id!\n", __func__);
		return -1;
	}

	int nret = 0;
	ltimer_opt_t optev;
	timer_bucket_t *p_bkt = (timer_bucket_t *)p_timer->p_bkt;
	
	memset(&optev, 0, sizeof(ltimer_opt_t));
	optev.opt = EN_TIMER_OPT_DEL;
	optev.id = timerid;
	nret = write(p_bkt->pipefd[1], &optev, sizeof(ltimer_opt_t));
	if (nret != sizeof(ltimer_opt_t)) {
		printf("%s: write pipe error!\n", __func__);
		return -1;
	}
	return 0;
}

const struct timespec* curtime(struct timespec* pts)
{
	if (NULL != pts) {
		pts->tv_sec = g_sys_curtime.tv_sec;
		pts->tv_nsec = g_sys_curtime.tv_nsec;
		return pts;
	}
	return &g_sys_curtime;
}

const char* curtime_str(char *timestr, int size)
{
	time_t tm = (time_t)g_sys_curtime.tv_sec;
	return (const char*)ctime_r(&tm, timestr);
}
