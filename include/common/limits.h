#pragma once

#define SHRT_MIN -32768
#define SHRT_MAX 32767	
#define USHRT_MAX	65535	
#define INT_MIN	-2147483648
#define INT_MAX	2147483647	
#define UINT_MAX 4294967295U

#define ULONG_MAX	((unsigned long)(~0L))
#define LONG_MAX ((long)(ULONG_MAX >> 1))
#define LONG_MIN ((long)(~LONG_MAX))