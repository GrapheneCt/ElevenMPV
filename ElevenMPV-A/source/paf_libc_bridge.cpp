#include <paf.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

extern "C" {

	void *memmove(void *str1, const void *str2, size_t n)
	{
		return sce_paf_memmove(str1, str2, n);
	}

	int memcmp(const void *str1, const void *str2, size_t n)
	{
		return sce_paf_memcmp(str1, str2, n);
	}

	size_t strlen(const char *str)
	{
		return sce_paf_strlen(str);
	}

	double ldexp(double x, int y)
	{
		return (float)ldexpf((float)x, y);
	}
}