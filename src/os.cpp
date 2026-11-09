/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/string.h>
#include <frost/os.h>

#if FROST_PLATFORM_WINDOWS
#	include <direct.h> // _wchdir
#elif !FROST_CRT_MSVC
#	include <unistd.h> // syscall, _SC_PAGESIZE
#endif // FROST_PLATFORM_WINDOWS

#if FROST_PLATFORM_WINDOWS || FROST_PLATFORM_WINRT
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif // WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	include <psapi.h>
#elif  FROST_PLATFORM_POSIX
#	include <sched.h> // sched_yield
#	if FROST_PLATFORM_IOS       \
	|| FROST_PLATFORM_OSX       \
	|| FROST_PLATFORM_PS4       \
	|| FROST_PLATFORM_PS5       \
	|| FROST_PLATFORM_VISIONOS
#		include <pthread.h> // mach_port_t
#	endif // FROST_PLATFORM_*

#	include <time.h> // nanosleep
#	if !FROST_PLATFORM_PS4 && !FROST_PLATFORM_PS5
#		include <dlfcn.h> // dlopen, dlclose, dlsym
#	endif // !FROST_PLATFORM_PS4 && !FROST_PLATFORM_PS5

#	if FROST_PLATFORM_ANDROID \
	|| FROST_PLATFORM_LINUX   \
	|| FROST_PLATFORM_RPI
#		include <stdio.h>  // fopen
#		include <sys/mman.h>
#		include <sys/syscall.h>
#	elif FROST_PLATFORM_IOS || FROST_PLATFORM_OSX
#		include <mach/mach.h> // mach_task_basic_info
#		include <sys/mman.h>
#	endif // FROST_PLATFORM_ANDROID
#endif // FROST_PLATFORM_

namespace frost
{
	void sleep(uint32_t _ms)
	{
#if FROST_PLATFORM_WINDOWS
		::Sleep(_ms);
#elif  FROST_PLATFORM_XBOXONE \
	|| FROST_PLATFORM_WINRT   \
	|| FROST_CRT_NONE
		FROST_UNUSED(_ms);
		FROST_ASSERT(false, "Function '%s' is not implemented!", FROST_FUNCTION);
#else
		timespec req = { (time_t)_ms/1000, (long)( (_ms%1000)*1000000) };
		timespec rem = { 0, 0 };
		::nanosleep(&req, &rem);
#endif // FROST_PLATFORM_
	}

	void yield()
	{
#if FROST_PLATFORM_WINDOWS
		::SwitchToThread();
#elif  FROST_PLATFORM_XBOXONE \
	|| FROST_PLATFORM_WINRT   \
	|| FROST_CRT_NONE
		FROST_ASSERT(false, "Function '%s' is not implemented!", FROST_FUNCTION);
#else
		::sched_yield();
#endif // FROST_PLATFORM_
	}

#if FROST_PLATFORM_WINDOWS
	typedef uint32_t (__stdcall* TimePeriodFn)(uint32_t _ms);

	static void timePeriod(const StringView& _name, uint32_t _ms)
	{
		if (0 == _ms)
		{
			return;
		}

		// Loaded on demand, so that users of frost don't have to link with winmm.
		static void* s_winmm = dlopen("winmm.dll");

		if (NULL != s_winmm)
		{
			TimePeriodFn fn = dlsym<TimePeriodFn>(s_winmm, _name);

			if (NULL != fn)
			{
				fn(_ms);
			}
		}
	}

	void timerResolutionBegin(uint32_t _ms)
	{
		timePeriod("timeBeginPeriod", _ms);
	}

	void timerResolutionEnd(uint32_t _ms)
	{
		timePeriod("timeEndPeriod", _ms);
	}
#else
	void timerResolutionBegin(uint32_t _ms)
	{
		FROST_UNUSED(_ms);
	}

	void timerResolutionEnd(uint32_t _ms)
	{
		FROST_UNUSED(_ms);
	}
#endif // FROST_PLATFORM_WINDOWS

	uint32_t getHardwareThreads()
	{
#if FROST_CRT_NONE
		return 1;
#elif FROST_PLATFORM_WINDOWS || FROST_PLATFORM_WINRT
		SYSTEM_INFO si;
		memSet(&si, 0, sizeof(si) );
		::GetNativeSystemInfo(&si);
		return max<uint32_t>(1, si.dwNumberOfProcessors);
#elif FROST_PLATFORM_POSIX && defined(_SC_NPROCESSORS_ONLN)
		const long num = ::sysconf(_SC_NPROCESSORS_ONLN);
		return uint32_t(max<long>(1, num) );
#else
		return 1;
#endif // FROST_PLATFORM_
	}

	uint32_t getTid()
	{
#if FROST_PLATFORM_WINDOWS
		return ::GetCurrentThreadId();
#elif  FROST_PLATFORM_ANDROID \
	|| FROST_PLATFORM_LINUX   \
	|| FROST_PLATFORM_RPI
		return (pid_t)::syscall(SYS_gettid);
#elif  FROST_PLATFORM_IOS \
	|| FROST_PLATFORM_OSX
		return (mach_port_t)::pthread_mach_thread_np(pthread_self() );
#else
		FROST_ASSERT(false, "Function '%s' is not implemented!", FROST_FUNCTION);
		return 0;
#endif // FROST_PLATFORM_
	}

	size_t getProcessMemoryUsed()
	{
#if FROST_PLATFORM_ANDROID || FROST_PLATFORM_LINUX
		FILE* file = fopen("/proc/self/statm", "r");
		if (NULL == file)
		{
			return 0;
		}

		long pages = 0;
		int items = fscanf(file, "%*s%ld", &pages);
		fclose(file);
		return 1 == items
			? pages * sysconf(_SC_PAGESIZE)
			: 0
			;
#elif FROST_PLATFORM_IOS || FROST_PLATFORM_OSX
#	if defined(MACH_TASK_BASIC_INFO)
		mach_task_basic_info info;
		mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;

		int const result = task_info(mach_task_self()
				, MACH_TASK_BASIC_INFO
				, (task_info_t)&info
				, &infoCount
				);
#	else
		task_basic_info info;
		mach_msg_type_number_t infoCount = TASK_BASIC_INFO_COUNT;

		int const result = task_info(mach_task_self()
				, TASK_BASIC_INFO
				, (task_info_t)&info
				, &infoCount
				);
#	endif // defined(MACH_TASK_BASIC_INFO)
		if (KERN_SUCCESS != result)
		{
			return 0;
		}

		return info.resident_size;
#elif FROST_PLATFORM_WINDOWS
		PROCESS_MEMORY_COUNTERS pmc;
		GetProcessMemoryInfo(GetCurrentProcess()
			, &pmc
			, sizeof(pmc)
			);
		return pmc.WorkingSetSize;
#else
		FROST_ASSERT(false, "Function '%s' is not implemented!", FROST_FUNCTION);
		return 0;
#endif // FROST_PLATFORM_*
	}

	void* dlopen(const FilePath& _filePath)
	{
#if FROST_PLATFORM_WINDOWS
		wchar_t filePath[kMaxFilePath];

		if (0 == toUtf16( (uint16_t*)filePath, FROST_COUNTOF(filePath), _filePath) )
		{
			return NULL;
		}

		// LOAD_LIBRARY_SEARCH_DEFAULT_DIRS excludes CWD and %PATH% from the search order.
		HMODULE handle = ::LoadLibraryExW(filePath, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

		FROST_WARN(NULL != handle, "dlopen failed: \"%s\" 0x%x.", _filePath.getCPtr(), ::GetLastError() );

		return (void*)handle;
#elif  FROST_PLATFORM_EMSCRIPTEN \
	|| FROST_PLATFORM_PS4        \
	|| FROST_PLATFORM_PS5        \
	|| FROST_PLATFORM_XBOXONE    \
	|| FROST_PLATFORM_WINRT      \
	|| FROST_PLATFORM_NX         \
	|| FROST_CRT_NONE
		FROST_UNUSED(_filePath);
		return NULL;
#else
		void* so = ::dlopen(_filePath.getCPtr(), RTLD_LOCAL|RTLD_LAZY);
		FROST_WARN(NULL != so, "dlopen failed: \"%s\" %s.", _filePath.getCPtr(), ::dlerror() );
		return so;
#endif // FROST_PLATFORM_
	}

	void dlclose(void* _handle)
	{
		if (NULL == _handle)
		{
			return;
		}

#if FROST_PLATFORM_WINDOWS
		::FreeLibrary( (HMODULE)_handle);
#elif  FROST_PLATFORM_EMSCRIPTEN \
	|| FROST_PLATFORM_PS4        \
	|| FROST_PLATFORM_PS5        \
	|| FROST_PLATFORM_XBOXONE    \
	|| FROST_PLATFORM_WINRT      \
	|| FROST_PLATFORM_NX         \
	|| FROST_CRT_NONE
		FROST_UNUSED(_handle);
#else
		::dlclose(_handle);
#endif // FROST_PLATFORM_
	}

	void* dlsym(void* _handle, const StringView& _symbol)
	{
		const int32_t symbolMax = _symbol.getLength()+1;
		char* symbol = (char*)FROST_STACK_ALLOC(symbolMax);
		strCopy(symbol, symbolMax, _symbol);

#if FROST_PLATFORM_WINDOWS
		return (void*)::GetProcAddress( (HMODULE)_handle, symbol);
#elif  FROST_PLATFORM_EMSCRIPTEN \
	|| FROST_PLATFORM_PS4        \
	|| FROST_PLATFORM_PS5        \
	|| FROST_PLATFORM_XBOXONE    \
	|| FROST_PLATFORM_WINRT      \
	|| FROST_PLATFORM_NX         \
	|| FROST_CRT_NONE
		FROST_UNUSED(_handle, symbol);
		return NULL;
#else
		return ::dlsym(_handle, symbol);
#endif // FROST_PLATFORM_
	}

	bool getEnv(char* _out, uint32_t* _inOutSize, const StringView& _name)
	{
		const int32_t nameMax = _name.getLength()+1;
		char* name = (char*)FROST_STACK_ALLOC(nameMax);
		strCopy(name, nameMax, _name);

#if FROST_PLATFORM_WINDOWS
		wchar_t* wname = (wchar_t*)FROST_STACK_ALLOC(nameMax*sizeof(wchar_t) );

		if (0 == toUtf16( (uint16_t*)wname, nameMax, name) )
		{
			*_inOutSize = 0;
			return false;
		}

		const uint32_t max = NULL != _out ? *_inOutSize : 0;
		wchar_t* value = (wchar_t*)FROST_STACK_ALLOC(max*sizeof(wchar_t) );

		DWORD len = ::GetEnvironmentVariableW(wname, value, max);
		bool result = len != 0 && len < max;

		if (result)
		{
			const int32_t num = fromUtf16(_out, int32_t(max), (const uint16_t*)value);
			result = 0 != num;
			len    = 0 != num ? num-1 : fromUtf16(NULL, 0, (const uint16_t*)value);
		}

		*_inOutSize = len;
		return result;
#elif  FROST_PLATFORM_EMSCRIPTEN \
	|| FROST_PLATFORM_PS4        \
	|| FROST_PLATFORM_PS5        \
	|| FROST_PLATFORM_XBOXONE    \
	|| FROST_PLATFORM_WINRT      \
	|| FROST_PLATFORM_NX         \
	|| FROST_CRT_NONE
		FROST_UNUSED(name, _out, _inOutSize);
		return false;
#else
		const char* ptr = ::getenv(name);
		uint32_t len = 0;
		bool result = false;
		if (NULL != ptr)
		{
			len = (uint32_t)strLen(ptr);

			result = len != 0 && len < *_inOutSize;
			if (len < *_inOutSize)
			{
				strCopy(_out, *_inOutSize, ptr);
			}
		}

		*_inOutSize = len;
		return result;
#endif // FROST_PLATFORM_
	}

	void setEnv(const StringView& _name, const StringView& _value)
	{
		const int32_t nameMax = _name.getLength()+1;
		char* name = (char*)FROST_STACK_ALLOC(nameMax);
		strCopy(name, nameMax, _name);

		char* value = NULL;
		if (!_value.isEmpty() )
		{
			int32_t valueMax = _value.getLength()+1;
			value = (char*)FROST_STACK_ALLOC(valueMax);
			strCopy(value, valueMax, _value);
		}

#if FROST_PLATFORM_WINDOWS
		wchar_t* wname  = (wchar_t*)FROST_STACK_ALLOC(nameMax*sizeof(wchar_t) );
		wchar_t* wvalue = NULL;

		if (0 == toUtf16( (uint16_t*)wname, nameMax, name) )
		{
			return;
		}

		if (NULL != value)
		{
			const int32_t valueMax = _value.getLength()+1;
			wvalue = (wchar_t*)FROST_STACK_ALLOC(valueMax*sizeof(wchar_t) );

			if (0 == toUtf16( (uint16_t*)wvalue, valueMax, value) )
			{
				return;
			}
		}

		::SetEnvironmentVariableW(wname, wvalue);
#elif  FROST_PLATFORM_EMSCRIPTEN \
	|| FROST_PLATFORM_PS4        \
	|| FROST_PLATFORM_PS5        \
	|| FROST_PLATFORM_XBOXONE    \
	|| FROST_PLATFORM_WINRT      \
	|| FROST_PLATFORM_NX         \
	|| FROST_CRT_NONE
		FROST_UNUSED(name, value);
#else
		if (NULL != value)
		{
			::setenv(name, value, 1);
		}
		else
		{
			::unsetenv(name);
		}
#endif // FROST_PLATFORM_
	}

	int chdir(const char* _path)
	{
#if FROST_PLATFORM_PS4     \
 || FROST_PLATFORM_PS5     \
 || FROST_PLATFORM_XBOXONE \
 || FROST_PLATFORM_WINRT   \
 || FROST_CRT_NONE
		FROST_UNUSED(_path);
		return -1;
#elif FROST_PLATFORM_WINDOWS
		wchar_t path[kMaxFilePath];

		if (0 == toUtf16( (uint16_t*)path, FROST_COUNTOF(path), _path) )
		{
			return -1;
		}

		return ::_wchdir(path);
#else
		return ::chdir(_path);
#endif // FROST_COMPILER_
	}

	void* exec(const char* const* _argv)
	{
#if FROST_PLATFORM_LINUX
		pid_t pid = fork();

		if (0 == pid)
		{
			int result = execvp(_argv[0], const_cast<char *const*>(&_argv[1]) );
			FROST_UNUSED(result);
			return NULL;
		}

		return (void*)uintptr_t(pid);
#elif FROST_PLATFORM_WINDOWS
		STARTUPINFOA si;
		memSet(&si, 0, sizeof(STARTUPINFOA) );
		si.cb = sizeof(STARTUPINFOA);

		PROCESS_INFORMATION pi;
		memSet(&pi, 0, sizeof(PROCESS_INFORMATION) );

		int32_t total = 0;
		for (uint32_t ii = 0; NULL != _argv[ii]; ++ii)
		{
			total += (int32_t)strLen(_argv[ii]) + 1;
		}

		char* temp = (char*)FROST_STACK_ALLOC(total);
		int32_t len = 0;
		for(uint32_t ii = 0; NULL != _argv[ii]; ++ii)
		{
			len += snprintf(&temp[len], frost::max(0, total-len)
				, "%s "
				, _argv[ii]
				);
		}

		bool ok = !!CreateProcessA(_argv[0]
			, temp
			, NULL
			, NULL
			, false
			, 0
			, NULL
			, NULL
			, &si
			, &pi
			);
		if (ok)
		{
			return pi.hProcess;
		}

		return NULL;
#else
		FROST_UNUSED(_argv);
		return NULL;
#endif // FROST_PLATFORM_LINUX
	}

	void exit(int32_t _exitCode, bool _cleanup)
	{
		if (_cleanup)
		{
			::exit(_exitCode);
		}

#if FROST_PLATFORM_WINDOWS
		TerminateProcess(GetCurrentProcess(), _exitCode);
#else
		_Exit(_exitCode);
#endif // FROST_PLATFORM_*

		FROST_UNREACHABLE;
	}

	namespace
	{
#if FROST_PLATFORM_ANDROID || FROST_PLATFORM_IOS || FROST_PLATFORM_LINUX || FROST_PLATFORM_OSX
		static int32_t toPosixProt(uint32_t _protect)
		{
			switch (_protect)
			{
			case Memory::ProtectRead:      return PROT_READ;
			case Memory::ProtectReadWrite: return PROT_READ | PROT_WRITE;
			case Memory::ProtectReadExec:  return PROT_READ | PROT_EXEC;
			case Memory::ProtectNone:      // fall through
			default:                       return PROT_NONE;
			}
		}

		static int32_t toPosixAdvice(uint32_t _advise)
		{
			switch (_advise)
			{
			case Memory::Normal:           return MADV_NORMAL;
			case Memory::SequentialAccess: return MADV_SEQUENTIAL;
			case Memory::RandomAccess:     return MADV_RANDOM;
			case Memory::Prefetch:         return MADV_WILLNEED;
			case Memory::DontNeed:         return MADV_DONTNEED;
			default:                       return -1;
			}
		}
#elif FROST_PLATFORM_WINDOWS
		static DWORD toWinProtect(uint32_t _protect)
		{
			switch (_protect)
			{
			case Memory::ProtectRead:      return PAGE_READONLY;
			case Memory::ProtectReadWrite: return PAGE_READWRITE;
			case Memory::ProtectReadExec:  return PAGE_EXECUTE_READ;
			case Memory::ProtectNone:      // fall through
			default:                       return PAGE_NOACCESS;
			}
		}
#endif // FROST_PLATFORM_*
	} // namespace

	void* memoryMap(void* _address, size_t _size, size_t _alignment, uint32_t _flags, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);

		const uint32_t state   = _flags & Memory::StateMask;
		const uint32_t protect = _flags & Memory::ProtectMask;
		const uint32_t advise  = _flags & Memory::AdviseMask;
		const uint32_t page    = _flags & Memory::PageMask;

		const size_t granularity = memoryAllocationGranularity();
		FROST_ASSERT(_alignment <= granularity, "Alignments greater than the allocation granularity are not implemented (requested %zu, granularity %zu).", _alignment, granularity);
		FROST_UNUSED(_alignment, granularity);

#if FROST_PLATFORM_ANDROID || FROST_PLATFORM_IOS || FROST_PLATFORM_LINUX || FROST_PLATFORM_OSX
		if (NULL == _address)
		{
			if (0 == (state & Memory::Reserve) )
			{
				FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: Memory::Reserve required for fresh allocation.");
				return NULL;
			}

			uint32_t effectiveProtect = protect;

			if (0 == effectiveProtect)
			{
				effectiveProtect = (Memory::Commit == (state & Memory::Commit) )
					? Memory::ProtectReadWrite
					: Memory::ProtectNone
					;
			}

			int32_t mmapFlags = 0
				| MAP_ANON
				| MAP_PRIVATE
				;

#	ifdef MAP_NORESERVE
			mmapFlags |= Memory::ProtectNone == effectiveProtect ? MAP_NORESERVE : 0;
#	endif // MAP_NORESERVE

#	if FROST_PLATFORM_LINUX
			if (0 != (page & Memory::PageHuge) )
			{
				mmapFlags |= MAP_HUGETLB;
#		ifdef MAP_HUGE_1GB
				mmapFlags |= MAP_HUGE_1GB;
#		endif // MAP_HUGE_1GB
			}
			else if (0 != (page & Memory::PageLarge) )
			{
				mmapFlags |= MAP_HUGETLB;
#		ifdef MAP_HUGE_2MB
				mmapFlags |= MAP_HUGE_2MB;
#		endif // MAP_HUGE_2MB
			}
#	else
			FROST_UNUSED(page);
#	endif // FROST_PLATFORM_LINUX

			void* result = mmap(_address, _size, toPosixProt(effectiveProtect), mmapFlags, -1 /*fd*/, 0 /*offset*/);

			if (MAP_FAILED == result)
			{
				FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: mmap failed.");
				return NULL;
			}

			if (0 != advise)
			{
				const int32_t adv = toPosixAdvice(advise);
				if (-1 != adv)
				{
					madvise(result, _size, adv);
				}
			}

			return result;
		}

		if (0 != (state & Memory::Decommit) )
		{
			madvise(_address, _size, MADV_DONTNEED);
			const int32_t prot = (0 != protect) ? toPosixProt(protect) : PROT_NONE;
			if (0 != mprotect(_address, _size, prot) )
			{
				FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: mprotect (decommit) failed.");
				return NULL;
			}
		}
		else if (Memory::Commit == (state & Memory::Commit) )
		{
			const uint32_t effectiveProtect = (0 != protect) ? protect : Memory::ProtectReadWrite;
			if (0 != mprotect(_address, _size, toPosixProt(effectiveProtect) ) )
			{
				FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: mprotect (commit) failed.");
				return NULL;
			}
		}
		else if (0 != protect)
		{
			if (0 != mprotect(_address, _size, toPosixProt(protect) ) )
			{
				FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: mprotect failed.");
				return NULL;
			}
		}

		if (0 != advise)
		{
			const int32_t adv = toPosixAdvice(advise);
			if (-1 != adv)
			{
				madvise(_address, _size, adv);
			}
		}

		return _address;

#elif FROST_PLATFORM_WINDOWS
		if (NULL == _address)
		{
			if (0 == (state & Memory::Reserve) )
			{
				FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: Memory::Reserve required for fresh allocation.");
				return NULL;
			}

			DWORD allocType = MEM_RESERVE;
			DWORD winProtect = PAGE_NOACCESS;

			if (Memory::Commit == (state & Memory::Commit) )
			{
				allocType |= MEM_COMMIT;
				winProtect = toWinProtect( (0 != protect) ? protect : Memory::ProtectReadWrite);
			}
			else if (0 != protect)
			{
				winProtect = toWinProtect(protect);
			}

			if (0 != (page & (Memory::PageLarge | Memory::PageHuge) ) )
			{
				allocType |= MEM_LARGE_PAGES;
			}

			void* result = VirtualAlloc(_address, _size, allocType, winProtect);

			if (NULL == result)
			{
				FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: VirtualAlloc failed.");
				return NULL;
			}

			FROST_UNUSED(advise);
			return result;
		}

		if (0 != (state & Memory::Decommit) )
		{
			if (!VirtualFree(_address, _size, MEM_DECOMMIT) )
			{
				FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: VirtualFree(MEM_DECOMMIT) failed.");
				return NULL;
			}
		}
		else if (Memory::Commit == (state & Memory::Commit) )
		{
			const DWORD winProtect = toWinProtect( (0 != protect) ? protect : Memory::ProtectReadWrite);
			if (NULL == VirtualAlloc(_address, _size, MEM_COMMIT, winProtect) )
			{
				FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: VirtualAlloc(MEM_COMMIT) failed.");
				return NULL;
			}
		}
		else if (0 != protect)
		{
			DWORD oldProtect = 0;
			if (!VirtualProtect(_address, _size, toWinProtect(protect), &oldProtect) )
			{
				FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: VirtualProtect failed.");
				return NULL;
			}
		}

		if (Memory::DontNeed == advise)
		{
			VirtualAlloc(_address, _size, MEM_RESET, PAGE_NOACCESS);
			VirtualUnlock(_address, _size);
		}

		return _address;
#else
		FROST_UNUSED(_address, _size, _alignment, _flags, state, protect, advise, page, granularity);
		FROST_ERROR_SET(_err, kErrorMemoryMapFailed, "memoryMap: Not implemented!");
		return NULL;
#endif // FROST_PLATFORM_*
	}

	void memoryUnmap(void* _address, size_t _size, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);

#if FROST_PLATFORM_ANDROID || FROST_PLATFORM_IOS || FROST_PLATFORM_LINUX || FROST_PLATFORM_OSX
		int32_t result = munmap(_address, _size);

		if (-1 == result)
		{
			FROST_ERROR_SET(
				  _err
				, kErrorMemoryUnmapFailed
				, "kErrorMemoryUnmapFailed"
				);
		}
#elif FROST_PLATFORM_WINDOWS
		FROST_UNUSED(_size);
		if (!VirtualFree(_address, 0, MEM_RELEASE) )
		{
			FROST_ERROR_SET(
				  _err
				, kErrorMemoryUnmapFailed
				, "kErrorMemoryUnmapFailed"
				);
		}
#else
		FROST_UNUSED(_address, _size);
		FROST_ERROR_SET(_err, kErrorMemoryUnmapFailed, "Not implemented!");
#endif // FROST_PLATFORM_*
	}

	size_t memoryPageSize(uint32_t _flags)
	{
		const uint32_t page = _flags & Memory::PageMask;

#if FROST_PLATFORM_ANDROID || FROST_PLATFORM_IOS || FROST_PLATFORM_LINUX || FROST_PLATFORM_OSX
		if (0 != (page & Memory::PageHuge) )
		{
			return 1ull << 30; // 1 GiB nominal
		}
		if (0 != (page & Memory::PageLarge) )
		{
			return 2ull << 20; // 2 MiB nominal
		}
		return sysconf(_SC_PAGESIZE);
#elif FROST_PLATFORM_WINDOWS
		if (0 != (page & (Memory::PageLarge | Memory::PageHuge) ) )
		{
			return ::GetLargePageMinimum();
		}
		SYSTEM_INFO si;
		memSet(&si, 0, sizeof(si) );
		::GetSystemInfo(&si);
		return si.dwPageSize;
#else
		FROST_UNUSED(page);
		return 16<<10;
#endif // FROST_PLATFORM_WINDOWS
	}

	size_t memoryAllocationGranularity()
	{
#if FROST_PLATFORM_WINDOWS
		SYSTEM_INFO si;
		memSet(&si, 0, sizeof(si) );
		::GetSystemInfo(&si);
		return si.dwAllocationGranularity;
#else
		return memoryPageSize();
#endif // FROST_PLATFORM_WINDOWS
	}

} // namespace frost
