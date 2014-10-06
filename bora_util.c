#include "bora_util.h"


uint64_t time_to_usec(struct timeval t)
{
	uint64_t usec;

	usec = t.tv_usec + (uint64_t)t.tv_sec * 1000000;
	return usec;
}

struct timeval time_from_usec(uint64_t usec)
{
	struct timeval t;

	t.tv_usec = usec % 1000000;
	t.tv_sec = usec / 1000000;
	return t;
}

struct timeval time_multiply(struct timeval t, unsigned long mult)
{
	return time_from_usec(time_to_usec(t) * mult);
}
