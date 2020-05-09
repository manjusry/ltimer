#ifndef LTIMER_H
#define LTIMER_H

#include <time.h>

//高精度海量定时器设计思路：

//定时器桶.
//定时器桶采用预分配内存，创建时需要指定桶的容量，定时器个数不超过桶的最大上限.
//定时器桶内的定时器用升序链表串起来，链表头部总是最近会超时的定时器.

//超时机制.
//每个定时器桶有一个工作线程.
//工作线程以固定频率触发，更新当前系统时间，并检查桶内是否有已超时的定时器，有则处理.
//工作线程监听的pipe读端有事件可读，读取出来并处理.

//定时器.
//超时时间用绝对时间.
//循环定时器超时，调用超时处理函数处理，完成后摘链，重新设置超时时间，再插入到升序链表尾部合适的位置.
//一次性定时器超时，调用超时处理函数处理，完成后摘链，放到回收链表.

//定时器的增删改.
//增加：从回收链表或者资源池中取出一个定时器并初始化，创建一个新增事件写入到pipe，由工作线程添加.
//删除：创建一个删除事件写入到pipe，由工作线程进行删除.
//修改：创建一个修改事件写入到pipe，由工作线程对定时器进行修改.

#ifdef __cplusplus
extern "C" {
#endif

typedef long TimerID_t;
typedef long TimerBucketID_t;
typedef void (*time_out_proc)(void* data);

//---------------------------------------------------------------------------------------
//@function: create a timer bucket.
//@param name: name of bucket, can be NULL, useful for debugging.
//@param size: max storage of bucket.
//@param cpuid: cpu core id to bind.
//@param resolution: resolution of this timer bucket.
//@return: -1 on error, positive number on success.
TimerBucketID_t create_timer_bucket(const char *name, int size, int cpuid, struct timespec resolution);

//@function: destroy timer bucket.
void destroy_timer_bucket(TimerBucketID_t bktid);

//@function : add timer to timer bucket.
//@param bktid: the timer bucket to add.
//@param tm: relative time to timeout.
//@param func: callback func when timeout.
//@param data: data for callback.
//@param type: 0 - once timer; 1 - cycle timer.
//@return: -1 on error, positive number on success.
TimerID_t add_timer(TimerBucketID_t bktid, struct timespec tm, time_out_proc func, void *data, int type);

//@function: modify timer by ID.
//@param tm: relative time to timeout.
//@param func: callback func when timeout.
//@param data: data to proc for callback.
//@return: -1 on error, 0 on success.
int mod_timer(TimerID_t timerid, struct timespec tm, time_out_proc func, void *data);

//@function: delete timer by ID.
//@return: -1 on error, 0 on success.
int del_timer(TimerID_t timerid);

//---------------------------------------------------------------------------------------
//@function: get current time.
const struct timespec* curtime(struct timespec* pts);

//@function: get current time str representation.
const char* curtime_str(char *timestr, int size);

#ifdef __cplusplus
}
#endif

#endif //LTIMER_H
