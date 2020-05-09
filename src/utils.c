#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>


#include "utils.h"

char* read_file_content(const char* file, int *p_readlen)
{
	struct stat fs;

	if (0 != (stat(file, &fs)) || !(fs.st_mode & S_IFREG)) {
		fprintf(stderr, "File [%s] does not exist.\n", file);
		return NULL;
	}

	char* content = (char *)calloc(fs.st_size + 1, sizeof(char));
	if (NULL == content) {
		return NULL;
	}

	FILE *fp = fopen(file, "r");
	if (NULL == fp) {
		fprintf(stderr, "Fopen file [%s] failed.\n", file);
		free(content);
		return NULL;
	}

	size_t readlen = fread(content, 1, fs.st_size, fp);
	content[readlen] = '\0';
	*p_readlen = readlen;

	fclose(fp);
	return content;
}

void release_file_content(char * ptr)
{
	if (NULL != ptr) {
		free(ptr);
	}
}

int set_thread_core_affinity(int core_id, pthread_t tid)
{
	cpu_set_t mask;

	int cores_can_affinity = sysconf(_SC_NPROCESSORS_CONF);
	if (core_id < cores_can_affinity) {
		CPU_ZERO(&mask);
		CPU_SET(core_id, &mask);
		return pthread_setaffinity_np(tid, sizeof(mask), &mask);
	}
	return -1;
}

int set_thread_name(const char *name, pthread_t tid)
{
	if (strlen(name) > 15) {
		printf("%s: name [%s] is too long!\n", __func__, name);
		return -1;
	}
	return pthread_setname_np(tid, name);
}

