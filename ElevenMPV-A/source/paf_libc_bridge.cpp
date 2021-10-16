#include <paf.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

extern "C" {

	static int s_fakeErrno = 0;

	void* memcpy(void* dest, const void* src, size_t count)
	{
		return sce_paf_memcpy(dest, src, count);
	}

	double ldexp(double x, int y)
	{
		return (float)ldexpf((float)x, y);
	}

	double sqrt(double x)
	{
		return (float)sqrtf((float)x);
	}

	int *_sceLibcErrnoLoc()
	{
		return &s_fakeErrno;
	}
}