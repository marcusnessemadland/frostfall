/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/timer.h>

#if FROST_CRT_NONE
#	include <frost/crt0.h>
#elif FROST_PLATFORM_ANDROID
#	include <time.h> // clock, clock_gettime
#elif FROST_PLATFORM_EMSCRIPTEN
#	include <emscripten.h>
#elif FROST_PLATFORM_WINDOWS || FROST_PLATFORM_XBOXONE || FROST_PLATFORM_WINRT
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif // WIN32_LEAN_AND_MEAN
#	include <windows.h>
#else
#	include <sys/time.h> // gettimeofday
#endif // FROST_PLATFORM_

namespace frost
{
	int64_t getHPCounter()
	{
#if    FROST_CRT_NONE
		int64_t i64 = crt0::getHPCounter();
#elif  FROST_PLATFORM_WINDOWS \
	|| FROST_PLATFORM_XBOXONE \
	|| FROST_PLATFORM_WINRT
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		int64_t i64 = li.QuadPart;
#elif FROST_PLATFORM_ANDROID
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		int64_t i64 = now.tv_sec*INT64_C(1000000000) + now.tv_nsec;
#elif FROST_PLATFORM_EMSCRIPTEN
		int64_t i64 = int64_t(1000.0f * emscripten_get_now() );
#elif !FROST_PLATFORM_NONE
		struct timeval now;
		gettimeofday(&now, NULL);
		int64_t i64 = now.tv_sec*INT64_C(1000000) + now.tv_usec;
#else
		FROST_ASSERT(false, "Not implemented!");
		int64_t i64 = UINT64_MAX;
#endif // FROST_PLATFORM_
		return i64;
	}

	int64_t getHPFrequency()
	{
#if    FROST_CRT_NONE
		return INT64_C(1000000000);
#elif  FROST_PLATFORM_WINDOWS \
	|| FROST_PLATFORM_XBOXONE \
	|| FROST_PLATFORM_WINRT
		LARGE_INTEGER li;
		QueryPerformanceFrequency(&li);
		return li.QuadPart;
#elif FROST_PLATFORM_ANDROID
		return INT64_C(1000000000);
#elif FROST_PLATFORM_EMSCRIPTEN
		return INT64_C(1000000);
#else
		return INT64_C(1000000);
#endif // FROST_PLATFORM_
	}

	const Ticks  Ticks::s_kStartup = getNow();
	const Ticks  Ticks::s_kFreq    = Ticks(getHPFrequency() );
	const double Ticks::s_kInvFreq = 1.0/double(Ticks::s_kFreq.ticks); 

} // namespace frost
