#ifndef UTILS_H 
#define UTILS_H
#include <pthread.h>

//--------------------------------------------------------------------------------------

#define SUCCESS          0  	/* Success! */
#define ERROR_COMMON    -1  	/* Generic error */
#define ERROR_NOMEM     -2  	/* Out of memory error */
#define ERROR_NODEV     -3  	/* No such device error */
#define ERROR_NOTSUP    -4  	/* Functionality is unsupported error */
#define ERROR_NOMOD     -5  	/* No module specified error */
#define ERROR_NOCTX     -6  	/* No context specified error */
#define ERROR_INVAL     -7  	/* Invalid argument/request error */
#define ERROR_EXISTS    -8  	/* Argument or device already exists */
#define ERROR_AGAIN     -9  	/* Try again */
#define READFILE_EOF    -42 	/* Hit the end of the file being read! */

//--------------------------------------------------------------------------------------

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define sys_ntohl(x) ((((x) & 0xFF000000)>>24) | (((x) & 0x00FF0000)>>8) | \
							(((x) & 0x0000FF00)<<8 ) | (((x) & 0x000000FF)<<24))
#define sys_ntohs(x)  ((((x) & 0xFF00)>>8) | (((x) & 0x00FF)<<8))
#define sys_htonl(x)  sys_ntohl(x)
#define sys_htons(x)  sys_ntohs(x)
#else  /* BIG_ENDIAN */
#define sys_htonl(x)  (x)
#define sys_ntohl(x)  (x)
#define sys_htons(x)  (x)
#define sys_ntohs(x)  (x)
#endif

//--------------------------------------------------------------------------------------

#define safe_add_4(val, off)  __sync_fetch_and_add(val, off)
#define safe_sub_4(val, off)  __sync_fetch_and_sub(val, off)
#define safe_zero_4(val)      __sync_lock_release(val)
#define safe_set_4(val, num)  __sync_lock_test_and_set(val, num)

#define safe_add_8(val, off)  __sync_fetch_and_add(val, off)
#define safe_sub_8(val, off)  __sync_fetch_and_sub(val, off)
#define safe_zero_8(val)      __sync_lock_release(val)
#define safe_set_8(val, num)  __sync_lock_test_and_set(val, num)

//--------------------------------------------------------------------------------------

char* read_file_content(const char* file, int *p_readlen);

void release_file_content(char * content);

int set_thread_core_affinity(int core_id, pthread_t tid);

int set_thread_name(const char *name, pthread_t tid);

#endif //UTILS_H
