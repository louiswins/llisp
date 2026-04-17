#include "perf.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static LARGE_INTEGER freq = { .QuadPart = 0 };

double gettime_perf() {
	if (freq.QuadPart == 0) {
		QueryPerformanceFrequency(&freq);
	}

	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return (double)t.QuadPart / (double)freq.QuadPart;
}
