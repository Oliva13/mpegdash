#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "list.h"
#include <sys/time.h>
#include "mpdutils.h"

unsigned long get_uptime(void)
{
	struct timespec tm;
	if (clock_gettime(CLOCK_MONOTONIC, &tm) != 0)
		return 0;
	return tm.tv_sec;
}

uint64_t GetCurrentTick()
{
	struct timespec tm;
	if (clock_gettime(CLOCK_MONOTONIC, &tm) != 0)
			return 0;
	return ((uint64_t)tm.tv_sec) * 1000 + tm.tv_nsec / 1000000;
}

void Sleep(unsigned int milliseconds)
{
	struct timespec request;
	request.tv_sec = milliseconds / 1000;
	request.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&request, NULL);
}

unsigned long long monotonic_ns(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;
}

unsigned long long monotonic_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

unsigned long long monotonic_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
}

unsigned monotonic_sec(void)
{
	return time(NULL);
}
















