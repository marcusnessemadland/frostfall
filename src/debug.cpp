/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/debug.h>
#include <frost/cpu.h>          // atomicCompareAndSwap
#include <frost/error.h>        // Error
#include <frost/math.h>         // min, clamp
#include <frost/string.h>       // isPrint
#include <frost/readerwriter.h> // WriterI, ErrorContext
#include <frost/os.h>           // exit
#include <frost/process.h>      // ProcessReader
#include <frost/scanner.h>      // Scanner

#include <inttypes.h>        // PRIx*

#if FROST_PLATFORM_LINUX || FROST_PLATFORM_OSX
#	include <cxxabi.h>   // abi::__cxa_demangle
#	include <execinfo.h> // backtrace, backtrace_symbols
#	include <unwind.h>   // _Unwind_Backtrace
#	include <dlfcn.h>

#	if FROST_PLATFORM_OSX
#		include <sys/sysctl.h> // sysctl
#		include <unistd.h>     // getpid
#	endif // FROST_PLATFORM_OSX
#endif // FROST_PLATFORM_*

#ifndef FROST_CONFIG_CALLSTACK_USE_EXECINFO
#	if defined(_EXECINFO_H) || defined(_EXECINFO_H_)
#		define FROST_CONFIG_CALLSTACK_USE_EXECINFO 1
#	else
#		define FROST_CONFIG_CALLSTACK_USE_EXECINFO 0
#	endif // defined...
#endif // FROST_CONFIG_CALLSTACK_USE_EXECINFO

#ifndef FROST_CONFIG_CALLSTACK_USE_UNWIND
#	if defined(_UNWIND_H) || defined(__UNWIND_H__)
#		define FROST_CONFIG_CALLSTACK_USE_UNWIND 1
#	else
#		define FROST_CONFIG_CALLSTACK_USE_UNWIND 0
#	endif // defined...
#endif // FROST_CONFIG_CALLSTACK_USE_UNWIND

#ifndef FROST_CONFIG_CALLSTACK_USE_LIB_BACKTRACE
#	define FROST_CONFIG_CALLSTACK_USE_LIB_BACKTRACE 0
#elif FROST_CONFIG_CALLSTACK_USE_LIB_BACKTRACE
#	if !FROST_PLATFORM_LINUX || !FROST_COMPILER_GCC
#		error "libbackrace is only supported on GCC/Linux."
#	endif // FROST_PLATFORM_LINUX && FROST_COMPILER_GCC
#endif // FROST_CONFIG_CALLSTACK_USE_LIB_BACKTRACE

#if FROST_CONFIG_CALLSTACK_USE_LIB_BACKTRACE
#	include <backtrace.h> // backtrace_syminfo
#endif // FROST_CONFIG_CALLSTACK_*

#ifndef FROST_CONFIG_CALLSTACK_USE_DBGHELP
#	define FROST_CONFIG_CALLSTACK_USE_DBGHELP FROST_PLATFORM_WINDOWS
#endif // FROST_CONFIG_CALLSTACK_USE_DBGHELP

#ifndef FROST_CONFIG_EXCEPTION_HANDLING_USE_WINDOWS_SEH
#	define FROST_CONFIG_EXCEPTION_HANDLING_USE_WINDOWS_SEH FROST_PLATFORM_WINDOWS
#endif // FROST_CONFIG_EXCEPTION_HANDLING_USE_WINDOWS_SEH

#ifndef FROST_CONFIG_EXCEPTION_HANDLING_USE_POSIX_SIGNALS
#	define FROST_CONFIG_EXCEPTION_HANDLING_USE_POSIX_SIGNALS ( (0 \
		| FROST_PLATFORM_LINUX                                    \
		| FROST_PLATFORM_OSX                                      \
		) && !FROST_CRT_NONE)
#endif // FROST_CONFIG_EXCEPTION_HANDLING_USE_POSIX_SIGNALS

#if FROST_CONFIG_EXCEPTION_HANDLING_USE_POSIX_SIGNALS
#	include <signal.h>
#endif // FROST_CONFIG_EXCEPTION_HANDLING_*

#if FROST_CONFIG_EXCEPTION_HANDLING_USE_WINDOWS_SEH && FROST_CRT_MSVC
#	include <stdlib.h> // _set_abort_behavior, _set_invalid_parameter_handler, _set_purecall_handler
#	include <crtdbg.h> // _CrtSetReportMode, _CrtSetReportHook
extern "C" __declspec(dllimport) unsigned int __stdcall SetErrorMode(unsigned int uMode);
#endif // FROST_CONFIG_EXCEPTION_HANDLING_USE_WINDOWS_SEH && FROST_CRT_MSVC

#if FROST_CRT_NONE
#	include <frost/crt0.h>
#elif FROST_PLATFORM_ANDROID
#	include <android/log.h>
#elif  FROST_PLATFORM_WINDOWS \
	|| FROST_PLATFORM_WINRT   \
	|| FROST_PLATFORM_XBOXONE
extern "C" __declspec(dllimport) int   __stdcall IsDebuggerPresent();
extern "C" __declspec(dllimport) void  __stdcall OutputDebugStringA(const char* _str);
extern "C" __declspec(dllimport) void* __stdcall GetStdHandle(unsigned long _stdHandle);
extern "C" __declspec(dllimport) int   __stdcall WriteFile(void* _file, const void* _buffer, unsigned long _sizeInBytes, unsigned long* _outNumberOfBytesWritten, struct _OVERLAPPED* _overlapped);
#elif FROST_PLATFORM_IOS || FROST_PLATFORM_OSX || FROST_PLATFORM_VISIONOS
#	if defined(__OBJC__)
#		import <Foundation/NSObjCRuntime.h>
#	else
#		include <CoreFoundation/CFString.h>
extern "C" void NSLog(CFStringRef _format, ...);
#	endif // defined(__OBJC__)
#elif FROST_PLATFORM_EMSCRIPTEN
#	include <emscripten/emscripten.h>
#else
#	include <stdio.h> // fputs, fflush
#endif // FROST_PLATFORM_WINDOWS

namespace frost
{
	void debugBreak()
	{
#if FROST_COMPILER_MSVC
		__debugbreak();
#elif FROST_CPU_ARM
		__builtin_trap();
//		asm("bkpt 0");
#elif FROST_CPU_RISCV && (FROST_COMPILER_GCC || FROST_COMPILER_CLANG)
		__asm__ volatile("ebreak");
#elif FROST_CPU_X86 && (FROST_COMPILER_GCC || FROST_COMPILER_CLANG)
		// NaCl doesn't like int 3:
		// NativeClient: NaCl module load failed: Validation failure. File violates Native Client safety rules.
		__asm__ ("int $3");
#elif FROST_PLATFORM_EMSCRIPTEN
		emscripten_log(0
			| EM_LOG_CONSOLE
			| EM_LOG_ERROR
			| EM_LOG_C_STACK
			| EM_LOG_JS_STACK
			, "debugBreak!"
			);
		// Doing emscripten_debugger() disables asm.js validation due to an emscripten bug
		//emscripten_debugger();
		EM_ASM({ debugger; });
#elif FROST_COMPILER_GCC || FROST_COMPILER_CLANG
		__builtin_trap();
#else
#	error "Unknown FROST_CPU_? / FROST_COMPILER_?"
#endif // FROST
	}

	bool isDebuggerPresent()
	{
#if FROST_PLATFORM_WINDOWS
		return 0 != ::IsDebuggerPresent();
#elif FROST_PLATFORM_OSX
		struct kinfo_proc info;
		size_t size = sizeof(info);
		int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };

		frost::memSet(&info, 0, sizeof(info) );

		if (0 != sysctl(mib, FROST_COUNTOF(mib), &info, &size, NULL, 0) )
		{
			return false;
		}

		return 0 != (info.kp_proc.p_flag & P_TRACED);
#else
		return false;
#endif // FROST_PLATFORM_*
	}

#if FROST_PLATFORM_WINDOWS \
 || FROST_PLATFORM_WINRT   \
 || FROST_PLATFORM_XBOXONE
	typedef void (*DebugOutputFn)(const char*);

	static void stubDebugOutput(const char* _out);

	static void*  s_debugConsoleHandle = NULL;
	static DebugOutputFn s_debugOutput = stubDebugOutput;

	static void debugOutputDbg(const char* _out)
	{
		OutputDebugStringA(_out);
	}

	static void debugOutputDbgAndConsole(const char* _out)
	{
		OutputDebugStringA(_out);

		unsigned long written;
		WriteFile(s_debugConsoleHandle, _out, (unsigned long)strLen(_out), &written, NULL);
	}

	static void stubDebugOutput(const char* _out)
	{
		void* handle = GetStdHandle(uint32_t(-12) /*STD_ERROR_HANDLE*/);
		s_debugConsoleHandle = (handle == (void*)-1)
			? NULL
			: handle
			;

		s_debugOutput = (NULL != s_debugConsoleHandle)
			? debugOutputDbgAndConsole
			: debugOutputDbg
			;
		s_debugOutput(_out);
	}
#endif // FROST_PLATFORM_*

	void debugOutput(const char* _out)
	{
#if FROST_CRT_NONE
		crt0::debugOutput(_out);
#elif FROST_PLATFORM_ANDROID
#	ifndef FROST_ANDROID_LOG_TAG
#		define FROST_ANDROID_LOG_TAG ""
#	endif // FROST_ANDROID_LOG_TAG
		__android_log_write(ANDROID_LOG_DEBUG, FROST_ANDROID_LOG_TAG, _out);
#elif  FROST_PLATFORM_WINDOWS \
	|| FROST_PLATFORM_WINRT   \
	|| FROST_PLATFORM_XBOXONE
		s_debugOutput(_out);
#elif  FROST_PLATFORM_IOS \
	|| FROST_PLATFORM_OSX   \
	|| FROST_PLATFORM_VISIONOS
#	if defined(__OBJC__)
		NSLog(@"%s", _out);
#	else
		NSLog(__CFStringMakeConstantString("%s"), _out);
#	endif // defined(__OBJC__)
#elif FROST_PLATFORM_EMSCRIPTEN
		emscripten_log(EM_LOG_CONSOLE, "%s", _out);
#else
		fputs(_out, stdout);
		fflush(stdout);
#endif // FROST_PLATFORM_
	}

	void debugOutput(const StringView& _str)
	{
#if FROST_CRT_NONE
		crt0::debugOutput(_str);
#else
		const char* data = _str.getPtr();
		int32_t size = _str.getLength();

		char temp[4096];
		while (0 != size)
		{
			uint32_t len = uint32_t(min(sizeof(temp)-1, size) );
			memCopy(temp, data, len);
			temp[len] = '\0';
			data += len;
			size -= len;
			debugOutput(temp);
		}
#endif // FROST_CRT_NONE
	}

	void debugPrintfVargs(const char* _format, va_list _argList)
	{
		char temp[8192];
		char* out = temp;
		int32_t len = vsnprintf(out, sizeof(temp), _format, _argList);
		if ( (int32_t)sizeof(temp) < len)
		{
			out = (char*)FROST_STACK_ALLOC(len+1);
			len = vsnprintf(out, len, _format, _argList);
		}
		out[len] = '\0';
		debugOutput(out);
	}

	void debugPrintf(const char* _format, ...)
	{
		va_list argList;
		va_start(argList, _format);
		debugPrintfVargs(_format, argList);
		va_end(argList);
	}

#define DBG_ADDRESS "%" PRIxPTR

	void debugPrintfData(const void* _data, uint32_t _size, const char* _format, ...)
	{
#define HEX_DUMP_WIDTH 16
#define HEX_DUMP_SPACE_WIDTH 48
#define HEX_DUMP_FORMAT "%-" FROST_STRINGIZE(HEX_DUMP_SPACE_WIDTH) "." FROST_STRINGIZE(HEX_DUMP_SPACE_WIDTH) "s"

		va_list argList;
		va_start(argList, _format);
		debugPrintfVargs(_format, argList);
		va_end(argList);

		debugPrintf("\ndata: " DBG_ADDRESS ", size: %d\n", _data, _size);

		if (NULL != _data)
		{
			const uint8_t* data = (const uint8_t*)_data;
			char hex[HEX_DUMP_WIDTH*3+1];
			char ascii[HEX_DUMP_WIDTH+1];
			uint32_t hexPos = 0;
			uint32_t asciiPos = 0;
			for (uint32_t ii = 0; ii < _size; ++ii)
			{
				snprintf(&hex[hexPos], sizeof(hex)-hexPos, "%02x ", data[asciiPos]);
				hexPos += 3;

				ascii[asciiPos] = isPrint(data[asciiPos]) ? data[asciiPos] : '.';
				asciiPos++;

				if (HEX_DUMP_WIDTH == asciiPos)
				{
					ascii[asciiPos] = '\0';
					debugPrintf("\t" DBG_ADDRESS "\t" HEX_DUMP_FORMAT "\t%s\n", data, hex, ascii);
					data += asciiPos;
					hexPos   = 0;
					asciiPos = 0;
				}
			}

			if (0 != asciiPos)
			{
				ascii[asciiPos] = '\0';
				debugPrintf("\t" DBG_ADDRESS "\t" HEX_DUMP_FORMAT "\t%s\n", data, hex, ascii);
			}
		}

#undef HEX_DUMP_WIDTH
#undef HEX_DUMP_SPACE_WIDTH
#undef HEX_DUMP_FORMAT
	}

	class DebugWriter : public WriterI
	{
		virtual int32_t write(const void* _data, int32_t _size, Error* _err) override
		{
			FROST_UNUSED(_err);
			debugOutput(StringView( (const char*)_data, _size) );
			return _size;
		}
	};

	WriterI* getDebugOut()
	{
		static DebugWriter s_debugOut;
		return &s_debugOut;
	}

#if FROST_CONFIG_CALLSTACK_USE_UNWIND
	struct UnwindCallbackData
	{
		uint32_t   skip;
		uint32_t   max;
		uint32_t   num;
		uintptr_t* outStack;
	};

	static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* _ctx, void* _arg)
	{
		UnwindCallbackData& ucd = *(UnwindCallbackData*)_arg;

		if (ucd.num < ucd.max)
		{
			if (0 < ucd.skip)
			{
				--ucd.skip;
				return _URC_NO_REASON;
			}

			uintptr_t addr = _Unwind_GetIP(_ctx);
			if (0 == addr)
			{
				return _URC_END_OF_STACK;
			}

			ucd.outStack[ucd.num++] = uintptr_t(addr);
			return _URC_NO_REASON;
		}

		return _URC_END_OF_STACK;
	}
#endif // FROST_CONFIG_CALLSTACK_USE_UNWIND

	FROST_NO_INLINE uint32_t getCallStackUnwind(uint32_t _skip, uint32_t _max, uintptr_t* _outStack)
	{
#if FROST_CONFIG_CALLSTACK_USE_UNWIND
		UnwindCallbackData ucd =
		{
			.skip     = _skip + 1,
			.max      = _max,
			.num      = 0,
			.outStack = _outStack,
		};

		_Unwind_Backtrace(unwindCallback, &ucd);

		return ucd.num;
#else
		FROST_UNUSED(_skip, _max, _outStack);
		return 0;
#endif // FROST_CONFIG_CALLSTACK_USE_UNWIND
	}

	FROST_NO_INLINE uint32_t getCallStackExecInfoBacktrace(uint32_t _skip, uint32_t _max, uintptr_t* _outStack)
	{
#if FROST_CONFIG_CALLSTACK_USE_EXECINFO
		const uint32_t max = _skip+_max+1;
		uintptr_t* tmp = (uintptr_t*)FROST_STACK_ALLOC(sizeof(uintptr_t)*max);

		const uint32_t numFull = backtrace( (void**)tmp, max);
		const uint32_t skip    = min(_skip + 1 /* skip self */, numFull);
		const uint32_t num     = numFull - skip;

		memCopy(_outStack, tmp + skip, sizeof(uintptr_t)*num);

		return num;
#else
		FROST_UNUSED(_skip, _max, _outStack);
		return 0;
#endif // FROST_CONFIG_CALLSTACK_USE_EXECINFO
	}

	inline const uintptr_t* nextStackFrame(const uintptr_t* _stackFrame)
	{
		const uintptr_t* newStackFrame = (const uintptr_t*)*_stackFrame;

		if (newStackFrame <= _stackFrame)
		{
			return NULL;
		}

		if (uintptr_t(newStackFrame) & (sizeof(uintptr_t) - 1) )
		{
			return NULL;
		}

		return newStackFrame;
	}

	FROST_NO_INLINE uint32_t getCallStackSystemVAbi(uint32_t _skip, uint32_t _max, uintptr_t* _outStack)
	{
		if constexpr (FROST_ENABLED( (FROST_PLATFORM_LINUX || FROST_PLATFORM_OSX) && FROST_ARCH_64BIT) )
		{
			const uintptr_t* stackFrame = (const uintptr_t*)FROST_STACK_FRAME_ADDRESS();

			uint32_t num = 0;

			while (NULL != stackFrame
			&&      num < _max)
			{
				if (uintptr_t(0) == stackFrame[1])
				{
					break;
				}

				if (FROST_UNLIKELY(0 < _skip) )
				{
					--_skip;
				}
				else
				{
					_outStack[num++] = stackFrame[1];
				}

				stackFrame = nextStackFrame(stackFrame);
			}

			return num;
		}
		else
		{
			return 0;
		}
	}

	FROST_NO_INLINE uint32_t getCallStackGccBuiltin(uint32_t _skip, uint32_t _max, uintptr_t* _outStack)
	{
#if FROST_COMPILER_GCC || FROST_COMPILER_CLANG
		FROST_PRAGMA_DIAGNOSTIC_PUSH();
		FROST_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wframe-address");

		uint32_t num = 0;

#define RETURN_ADDRESS(_x)                                 \
		if (num < _max)                                    \
		{                                                  \
			if (0 < _skip)                                 \
			{                                              \
				--_skip;                                   \
			}                                              \
			else                                           \
			{                                              \
				if (NULL == __builtin_frame_address(_x) )  \
				{                                          \
					return num;                            \
				}                                          \
				                                           \
				void* addr = __builtin_return_address(_x); \
				                                           \
				if (NULL == addr)                          \
				{                                          \
					return num;                            \
				}                                          \
				                                           \
				_outStack[num++] = uintptr_t(addr);        \
			}                                              \
		}                                                  \
		else                                               \
		{                                                  \
			return num;                                    \
		}

		RETURN_ADDRESS(0);
		RETURN_ADDRESS(1);
		RETURN_ADDRESS(2);
		RETURN_ADDRESS(3);
		RETURN_ADDRESS(4);
		RETURN_ADDRESS(5);
		RETURN_ADDRESS(6);
		RETURN_ADDRESS(7);
		RETURN_ADDRESS(8);
		RETURN_ADDRESS(9);

		RETURN_ADDRESS(10);
		RETURN_ADDRESS(11);
		RETURN_ADDRESS(12);
		RETURN_ADDRESS(13);
		RETURN_ADDRESS(14);
		RETURN_ADDRESS(15);
		RETURN_ADDRESS(16);
		RETURN_ADDRESS(17);
		RETURN_ADDRESS(18);
		RETURN_ADDRESS(19);

		RETURN_ADDRESS(20);
		RETURN_ADDRESS(21);
		RETURN_ADDRESS(22);
		RETURN_ADDRESS(23);
		RETURN_ADDRESS(24);
		RETURN_ADDRESS(25);
		RETURN_ADDRESS(26);
		RETURN_ADDRESS(27);
		RETURN_ADDRESS(28);
		RETURN_ADDRESS(29);

		RETURN_ADDRESS(30);
		RETURN_ADDRESS(31);

#undef RETURN_ADDRESS

		FROST_PRAGMA_DIAGNOSTIC_POP();

		return num;
#else
		FROST_UNUSED(_skip, _max, _outStack);
		return 0;
#endif // FROST_COMPILER_GCC || FROST_COMPILER_CLANG
	}

#if !FROST_PLATFORM_WINDOWS
#	define __stdcall
#endif // !FROST_PLATFORM_WINDOWS

	typedef uint16_t (__stdcall* RtlCaptureStackBackTraceFn)(uint32_t _framesToSkip, uint32_t _framesToCapture, void** _outBacktrace, uint32_t* _outhash);

	uint16_t __stdcall stubTryLoadRtlCaptureStackBackTrace(uint32_t _framesToSkip, uint32_t _framesToCapture, void** _outBacktrace, uint32_t* _outhash);
	RtlCaptureStackBackTraceFn RtlCaptureStackBackTrace = stubTryLoadRtlCaptureStackBackTrace;

	uint16_t __stdcall stubRtlCaptureStackBackTrace(uint32_t _framesToSkip, uint32_t _framesToCapture, void** _outBacktrace, uint32_t* _outhash)
	{
		FROST_UNUSED(_framesToSkip, _framesToCapture, _outBacktrace, _outhash);
		return 0;
	}

	uint16_t __stdcall stubTryLoadRtlCaptureStackBackTrace(uint32_t _framesToSkip, uint32_t _framesToCapture, void** _outBacktrace, uint32_t* _outhash)
	{
		void* kernel32Dll = frost::dlopen("kernel32.dll");

		RtlCaptureStackBackTraceFn fn = NULL != kernel32Dll
			? frost::dlsym<RtlCaptureStackBackTraceFn>(kernel32Dll, "RtlCaptureStackBackTrace")
			: NULL
			;

		RtlCaptureStackBackTrace = NULL != fn
			? fn
			: stubRtlCaptureStackBackTrace
			;

		return RtlCaptureStackBackTrace(_framesToSkip, _framesToCapture, _outBacktrace, _outhash);
	}

	FROST_NO_INLINE uint32_t getCallStackWinRtl(uint32_t _skip, uint32_t _max, uintptr_t* _outStack)
	{
#if FROST_PLATFORM_WINDOWS
		return RtlCaptureStackBackTrace(_skip + 1 /* skip self */, _max, (void**)_outStack, NULL);
#else
		FROST_UNUSED(_skip, _max, _outStack);
		return 0;
#endif // FROST_PLATFORM_WINDOWS
	}

	FROST_NO_INLINE uint32_t getCallStackWinAbi(uint32_t _skip, uint32_t _max, uintptr_t* _outStack)
	{
		if constexpr (FROST_ENABLED(FROST_PLATFORM_WINDOWS && FROST_CPU_X86 && FROST_ARCH_32BIT) )
		{
			const uintptr_t* stackFrame = (uintptr_t*)&_skip - 2;

			uint32_t num = 0;

			while (NULL != stackFrame
			&&      num < _max)
			{
				if (uintptr_t(0) == stackFrame[1])
				{
					break;
				}

				if (FROST_UNLIKELY(0 < _skip) )
				{
					--_skip;
				}
				else
				{
					_outStack[num++] = stackFrame[1];
				}

				stackFrame = nextStackFrame(stackFrame);
			}

			return num;
		}
		else if constexpr (FROST_ENABLED(FROST_PLATFORM_WINDOWS && FROST_ARCH_64BIT) )
		{
			return getCallStackWinRtl(_skip + 1 /* skip self */, _max, _outStack);
		}
		else
		{
			return 0;
		}
	}

	FROST_NO_INLINE uint32_t getCallStackFast(uint32_t _skip, uint32_t _max, uintptr_t* _outStack)
	{
#if FROST_PLATFORM_LINUX || FROST_PLATFORM_OSX
		return getCallStackSystemVAbi(_skip + 1 /* skip self */, _max, _outStack);
#elif FROST_PLATFORM_WINDOWS
		return getCallStackWinAbi(_skip + 1 /* skip self */, _max, _outStack);
#elif FROST_COMPILER_GCC || FROST_COMPILER_CLANG
		return getCallStackGccBuiltin(_skip + 1 /* skip self */, _max, _outStack);
#else
		FROST_UNUSED(_skip, _max, _outStack);
		return 0;
#endif // FROST_PLATFORM_*
	}

	FROST_NO_INLINE uint32_t getCallStackExact(uint32_t _skip, uint32_t _max, uintptr_t* _outStack)
	{
#if FROST_PLATFORM_LINUX
		return getCallStackUnwind(_skip + 1 /* skip self */, _max, _outStack);
#elif FROST_PLATFORM_OSX
		return getCallStackExecInfoBacktrace(_skip + 1 /* skip self */, _max, _outStack);
#elif FROST_PLATFORM_WINDOWS
		return getCallStackWinRtl(_skip + 1 /* skip self */, _max, _outStack);
#else
		FROST_UNUSED(_skip, _max, _outStack);
		return 0;
#endif // FROST_PLATFORM_*
	}

	static constexpr uint32_t kWidth   = 80;
	static constexpr uint32_t kWidthPc = 16;

#if FROST_CONFIG_CALLSTACK_USE_LIB_BACKTRACE
	struct StackTraceContext
	{
		StackTraceContext()
		{
			state = backtrace_create_state(NULL, 0, NULL, NULL);
		}

		struct backtrace_state* state;
	};

	static StackTraceContext s_stCtx;

	struct CallbackData
	{
		StringView resolvedName;
		StringView fileName;
		int32_t line;
	};

	static void backtraceSymInfoCb(void* _data, uintptr_t _pc, const char* _symName, uintptr_t _symVal, uintptr_t _symSize)
	{
		FROST_UNUSED(_pc, _symVal);

		CallbackData* cbData = (CallbackData*)_data;
		cbData->resolvedName.set(_symName, _symSize);
	}

	static int backtraceFullCb(void* _data, uintptr_t _pc, const char* _fileName, int32_t _lineNo, const char* _function)
	{
		FROST_UNUSED(_pc, _function);

		CallbackData* cbData = (CallbackData*)_data;
		if (NULL == _fileName)
		{
			cbData->fileName.set("<Unknown?>");
			cbData->line = -1;
		}
		else
		{
			cbData->fileName.set(_fileName);
			cbData->line = _lineNo;
		}

		return 1;
	}

	int32_t writeCallstack(WriterI* _writer, const uintptr_t* _stack, uint32_t _num, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);

		char   demangleBuf[4096];
		size_t demangleLen = FROST_COUNTOF(demangleBuf);

		int32_t total = write(_writer, _err, "Callstack (%d):\n", _num);

		total += write(_writer, _err, "\t #: %-*s   Line  %-*s  Function ---\n", kWidth, "File ---", kWidthPc, "PC ---");

		CallbackData cbData;

		for (uint32_t ii = 0; ii < _num && _err->isOk(); ++ii)
		{
			backtrace_pcinfo(s_stCtx.state, _stack[ii], backtraceFullCb, NULL, &cbData);

			StringView demangledName = "<Unknown?>";

			if (1 == backtrace_syminfo(s_stCtx.state, _stack[ii], backtraceSymInfoCb, NULL, &cbData)
			&&  !cbData.resolvedName.isEmpty() )
			{
				demangleLen = FROST_COUNTOF(demangleBuf);
				int32_t demangleStatus;
				abi::__cxa_demangle(cbData.resolvedName.getPtr(), demangleBuf, &demangleLen, &demangleStatus);

				if (0 == demangleStatus)
				{
					demangledName.set({demangleBuf, narrowCast<int32_t>(demangleLen) });
				}
			}

			const StringView fileName = strTail(cbData.fileName, kWidth);

			total += write(_writer, _err
				, "\t%2d: %-*S % 6d  %*p  %S\n"
				, ii
				, kWidth
				, &fileName
				, cbData.line
				, kWidthPc
				, _stack[ii]
				, &demangledName
				);

			if (0 == strCmp(demangledName, "main", 4) )
			{
				if (0 != _num-1-ii)
				{
					total += write(_writer, _err
						, "\t... %d more stack frames below 'main'.\n"
						, _num-1-ii
						);
				}
				break;
			}
		}

		return total;
	}

#elif FROST_CONFIG_CALLSTACK_USE_DBGHELP

	struct DbgHelpSymbolInfo
	{
		uint32_t SizeOfStruct;
		uint32_t TypeIndex;
		uint64_t Reserved[2];
		uint32_t Index;
		uint32_t Size;
		uint64_t ModBase;
		uint32_t Flags;
		uint64_t Value;
		uint64_t Address;
		uint32_t Register;
		uint32_t Scope;
		uint32_t Tag;
		uint32_t NameLen;
		uint32_t MaxNameLen;
		char     Name[512];
	};

	struct ImageHlpLine
	{
		uint32_t    SizeOfStruct;
		void*       Key;
		uint32_t    LineNumber;
		const char* FileName;
		uintptr_t   Address;
	};

	typedef bool (__stdcall* SymInitializeFn)(void* _process, const char* _userSearchPath, bool _invadeProcess);
	typedef bool (__stdcall* SymCleanupFn)(void* _process);
	typedef bool (__stdcall* SymFromAddrFn)(void* _process, uint64_t _address, uint64_t* _outDisplacement, DbgHelpSymbolInfo* _inoutSymbol);
	typedef bool (__stdcall* SymGetLineFromAddrFn)(void* _process, uintptr_t _address, uint32_t* _outDisplacement, ImageHlpLine* _inoutLine);

	constexpr uint32_t kSymOptUndName       = 0x00000002;
	constexpr uint32_t kSymOptDeferredLoads = 0x00000004;
	constexpr uint32_t kSymOptLoadLines     = 0x00000010;

	static void* kCurrentProcess = (void*)UINTPTR_MAX;

	typedef uint32_t (__stdcall* SymSetOptionsFn)(uint32_t _options);

	struct DbgHelpSymbolResolve
	{
		void init()
		{
			m_imageHlpDll = dlopen("dbghelp.dll");

			if (NULL != m_imageHlpDll)
			{
				auto symInitialize      = dlsym<SymInitializeFn     >(m_imageHlpDll, "SymInitialize");
				auto symSetOptions      = dlsym<SymSetOptionsFn     >(m_imageHlpDll, "SymSetOptions");
				auto symCleanup         = dlsym<SymCleanupFn        >(m_imageHlpDll, "SymCleanup");
				auto symFromAddr        = dlsym<SymFromAddrFn       >(m_imageHlpDll, "SymFromAddr");
				auto symGetLineFromAddr = dlsym<SymGetLineFromAddrFn>(m_imageHlpDll, FROST_ARCH_32BIT ? "SymGetLineFromAddr" : "SymGetLineFromAddr64");

				if (true
				&& NULL != symInitialize
				&& NULL != symSetOptions
				&& NULL != symCleanup
				&& NULL != symFromAddr
				&& NULL != symGetLineFromAddr
				   )
				{
					symSetOptions(kSymOptUndName | kSymOptDeferredLoads | kSymOptLoadLines);

					char symCache[1024] = "SRV*";
					uint32_t len = FROST_COUNTOF(symCache) - 4;
					if (getEnv(symCache + 4, &len, "TEMP") )
					{
						len += 4;
						snprintf(symCache + len, FROST_COUNTOF(symCache) - len, "\\SymbolCache*http://msdl.microsoft.com/download/symbols;");
					}
					else
					{
						symCache[0] = '\0';
					}

					if (symInitialize(kCurrentProcess, symCache, true) )
					{
						m_symCleanup         = symCleanup;
						m_symFromAddr        = symFromAddr;
						m_symGetLineFromAddr = symGetLineFromAddr;
					}
					else
					{
						FROST_TRACE("Failed to initialize debug symbols!");
						cleanup();
					}
				}
				else
				{
					FROST_TRACE("Failed to obtain debug symbols functions!");
					cleanup();
				}
			}
		}

		~DbgHelpSymbolResolve()
		{
			cleanup();
		}

		bool hasSymbols() const
		{
			return NULL != m_imageHlpDll;
		}

		bool resolve(uintptr_t _addr, FixedString1024& _outFunctionName, FilePath& _outFilePath, int32_t& _outLine) const
		{
			if (hasSymbols() )
			{
				DbgHelpSymbolInfo dhsi = {};
				dhsi.SizeOfStruct = sizeof(DbgHelpSymbolInfo) - sizeof(dhsi.Name);
				dhsi.MaxNameLen   = sizeof(dhsi.Name);

				ImageHlpLine ihl = {};
				ihl.SizeOfStruct = sizeof(ImageHlpLine);

				uint32_t lineDisplacement;

				if (true
				&&  m_symFromAddr(kCurrentProcess, _addr, NULL, &dhsi)
				&&  m_symGetLineFromAddr(kCurrentProcess, _addr, &lineDisplacement, &ihl)
				   )
				{
					_outFunctionName.set(dhsi.Name);
					_outFilePath.set(ihl.FileName);
					_outLine = ihl.LineNumber;

					return true;
				}
			}

			return false;
		}

		void cleanup()
		{
			if (NULL != m_imageHlpDll)
			{
				if (NULL != m_symCleanup)
				{
					m_symCleanup(kCurrentProcess);
				}

				m_symCleanup         = NULL;
				m_symFromAddr        = NULL;
				m_symGetLineFromAddr = NULL;

				dlclose(m_imageHlpDll);
				m_imageHlpDll = NULL;
			}
		}

		void* m_imageHlpDll = NULL;

		SymCleanupFn         m_symCleanup         = NULL;
		SymFromAddrFn        m_symFromAddr        = NULL;
		SymGetLineFromAddrFn m_symGetLineFromAddr = NULL;
	};

	static DbgHelpSymbolResolve s_dbgHelpSymbolResolve;

	int32_t writeCallstack(WriterI* _writer, const uintptr_t* _stack, uint32_t _num, Error* _err)
	{
		int32_t total = write(_writer, _err, "Callstack (%d):\n", _num);

		total += write(_writer, _err, "\t #: %-*s   Line  %-*s  Function ---\n", kWidth, "File ---", kWidthPc, "PC ---");

		static bool initCalled = false;

		if (!initCalled)
		{
			initCalled = true;

			s_dbgHelpSymbolResolve.init();
		}

		for (uint32_t ii = 0; ii < _num && _err->isOk(); ++ii)
		{
			FixedString1024 functionName;
			FilePath filePath;
			int32_t line;

			if (!s_dbgHelpSymbolResolve.resolve(_stack[ii], functionName, filePath, line))
			{
				functionName = "<Unknown?>";
				filePath     = "<Unknown?>";
				line         = -1;
			}

			const StringView fileName = strTail(filePath, kWidth);

			total += write(_writer, _err
				, "\t%2d: %-*S % 6d  %*p  %s\n"
				, ii
				, kWidth
				, &fileName
				, line
				, kWidthPc
				, _stack[ii]
				, functionName.getCPtr()
				);
		}

		return total;
	}

#elif FROST_CONFIG_CALLSTACK_USE_EXECINFO

	int32_t writeCallstack(WriterI* _writer, const uintptr_t* _stack, uint32_t _num, Error* _err)
	{
		int32_t total = write(_writer, _err, "Callstack (%d):\n", _num);

		// Linux:
		// <executable>([function]<+offset>)<space>[<address>]
		//
		// macOS:
		// <index><space><executable><space><address><space><function><space>+<space><offset>
		char** symbols = backtrace_symbols( (void* const*) _stack, _num);

#	if FROST_PLATFORM_LINUX

		struct Addr2Line
		{
			StringView functionName;
			StringView filePath;
			int32_t    line;
		};

		char addr2LineBuffer[32<<10];
		Addr2Line* addr2Line = (Addr2Line*)FROST_STACK_ALLOC(_num * sizeof(Addr2Line) );

		{
			FixedStringT<4096> args;
			FilePath first;

			for (uint32_t ii = 0; ii < _num; ++ii)
			{
				Addr2Line& a2l = addr2Line[ii];

				a2l.functionName = "<Unknown?>";
				a2l.filePath     = "<Unknown?>";
				a2l.line         = -1;
			}

			for (uint32_t ii = 0; ii < _num; ++ii)
			{
				const StringView line(symbols[ii]);

				const StringView parenS = strFind(line, '(');

				if (parenS.isEmpty() )
				{
					continue;
				}

				const FilePath filePath({ line.getPtr(), parenS.getPtr() });

				StringView offset(parenS.getTerm(), line.getTerm() );
				const StringView parenE = strFind(offset, ')');

				if (parenE.isEmpty() )
				{
					continue;
				}

				offset.set(parenS.getTerm(), parenE.getPtr() );

				if (first.isEmpty() )
				{
					first.set(filePath);

					args.append("-C -f -e ");
					args.append(filePath);
				}

				if (first != filePath)
				{
					continue;
				}

				args.append(" ");
				args.append(offset);
			}

			{
				ProcessReader reader;
				if (open(&reader, "addr2line", args, ErrorIgnore{}) )
				{
					int32_t bytes = read(&reader, addr2LineBuffer, sizeof(addr2LineBuffer), ErrorIgnore{});

					if (-1 != bytes)
					{
						uint32_t ii = 0;
						for (LineReader lr({addr2LineBuffer, bytes}); !lr.isDone(); ++ii)
						{
							const StringView functionName = lr.next();

							const StringView filePath = lr.next();

							const StringView colon = strFind(filePath, ':');

							if (!colon.isEmpty() )
							{
								Addr2Line& a2l = addr2Line[ii];

								a2l.functionName = functionName;
								a2l.filePath     = StringView(filePath.getPtr(), colon.getPtr() );
								fromString(&a2l.line, StringView(strWord({ colon.getTerm(), filePath.getTerm() }) ) );
							}
						}
					}

					close(&reader);
				}
			}
		}
#	elif FROST_PLATFORM_OSX

		char atosBuffer[32<<10];

#	endif // FROST_PLATFORM_*

		total += write(_writer, _err, "\t #: %-*s   Line  %-*s  Function ---\n", kWidth, "File ---", kWidthPc, "PC ---");

		for (uint32_t ii = 0; ii < _num && _err->isOk(); ++ii)
		{
			FixedString1024 functionName;
			FilePath filePath("<Unknown?>");
			int32_t line = -1;

#	if FROST_PLATFORM_LINUX
			filePath = addr2Line[ii].filePath;
			line = addr2Line[ii].line;
			functionName = addr2Line[ii].functionName;

#	elif FROST_PLATFORM_OSX
			StringView atosFunctionName("<Unknown?>");

			functionName.set(
				strWord(
				strLTrimSpace(
				strLTrimNonSpace(
				strLTrimSpace(
				strLTrimNonSpace(
				strLTrimSpace(
				strLTrimNonSpace(
					symbols[ii]
					) ) ) ) ) ) )
					);

			Dl_info info;
			dladdr( (const void*)_stack[ii], &info);

			{
				char args[4096];

				snprintf(args, FROST_COUNTOF(args), "--fullPath -o %s -l %x %x"
					, info.dli_fname
					, info.dli_fbase
					, _stack[ii]
					);

				ProcessReader reader;
				if (open(&reader, "atos", args, ErrorIgnore{}) )
				{
					int32_t bytes = read(&reader, atosBuffer, sizeof(atosBuffer), ErrorIgnore{});

					if (-1 != bytes)
					{
						for (LineReader lr({atosBuffer, bytes}); !lr.isDone();)
						{
							Scanner scanner(lr.next() );

							atosFunctionName = scanner.acceptUntil(" (");

							if (atosFunctionName.isEmpty() )
							{
								break;
							}

							scanner.accept(" (");

							filePath = scanner.acceptUntil(":");

							if (filePath.isEmpty() )
							{
								filePath = "<Unknown?>";
								break;
							}

							scanner.accept(':');

							const StringView lineStr = scanner.acceptUntil(")");

							if (!lineStr.isEmpty() )
							{
								fromString(&line, lineStr);
							}
						}
					}

					close(&reader);
				}
			}

			char    demangleBuf[4096];
			size_t  demangleLen = FROST_COUNTOF(demangleBuf);
			int32_t demangleStatus;
			abi::__cxa_demangle(functionName.getCPtr(), demangleBuf, &demangleLen, &demangleStatus);

			if (0 == demangleStatus)
			{
				functionName.set({demangleBuf, narrowCast<int32_t>(demangleLen) });
			}
			else
			{
				functionName = atosFunctionName;
			}

#	endif // FROST_PLATFORM_*

			const StringView fileName = strTail(filePath, kWidth);

			total += write(_writer, _err
				, "\t%2d: %-*S % 6d  %*p  %s\n"
				, ii
				, kWidth
				, &fileName
				, line
				, kWidthPc
				, _stack[ii]
				, functionName.getCPtr()
				);
		}

		/*backtrace_symbols_*/::free(symbols);

		return total;
	}

#else

	int32_t writeCallstack(WriterI* _writer, const uintptr_t* _stack, uint32_t _num, Error* _err)
	{
		int32_t total = write(_writer, _err, "Callstack (%d): - symbol resolve is not available\n", _num);

		total += write(_writer, _err, "\t #: %-*s   Line  %-*s  Function ---\n", kWidth, "File ---", kWidthPc, "PC ---");

		const StringView fileName      = "<unknown?>";
		const StringView demangledName = "<unknown?>";

		for (uint32_t ii = 0; ii < _num && _err->isOk(); ++ii)
		{
			total += write(_writer, _err
				, "\t%2d: %-*S % 6d  %*p  %S\n"
				, ii
				, kWidth
				, &fileName
				, -1
				, kWidthPc
				, _stack[ii]
				, &demangledName
				);
		}

		return total;
	}

#endif // FROST_CONFIG_CALLSTACK_*

	void debugOutputCallstack(uint32_t _skip)
	{
		char temp[8192];
		StaticMemoryBlockWriter smb(temp, FROST_COUNTOF(temp) );

		uintptr_t stack[32];
		const uint32_t  num = getCallStackExact(_skip + 1 /* skip self */, FROST_COUNTOF(stack), stack);
		const int32_t total = writeCallstack(&smb, stack, num, ErrorIgnore{});

		write(getDebugOut(), temp, total, ErrorIgnore{});
	}

#if FROST_CONFIG_EXCEPTION_HANDLING_USE_POSIX_SIGNALS
	struct SignalInfo
	{
		int32_t signalId;
		const char* name;
	};

	static const SignalInfo s_signalInfo[] =
	{   // Linux
		{ /*  4 */ SIGILL,  "SIGILL - Illegal instruction signal."     },
		{ /*  6 */ SIGABRT, "SIGABRT - Abort signal."                  },
		{ /*  8 */ SIGFPE,  "SIGFPE - Floating point error signal."    },
		{ /* 11 */ SIGSEGV, "SIGSEGV - Segmentation violation signal." },
	};

	struct ExceptionHandler
	{
		ExceptionHandler()
		{
			FROST_TRACE("ExceptionHandler - POSIX");

			stack_t stack = {};
			stack.ss_sp    = s_stack;
			stack.ss_size  = sizeof(s_stack);
			stack.ss_flags = 0;
			sigaltstack(&stack, &m_oldStack);

			struct sigaction sa = {};
			sa.sa_handler   = NULL;
			sa.sa_sigaction = signalActionHandler;
			sa.sa_mask      = { 0 };
			sa.sa_flags     = SA_ONSTACK | SA_SIGINFO;

			for (uint32_t ii = 0; ii < FROST_COUNTOF(s_signalInfo); ++ii)
			{
				sigaction(s_signalInfo[ii].signalId, &sa, &m_oldSignalAction[ii]);
			}
		}

		~ExceptionHandler()
		{
		}

		static void signalActionHandler(int32_t _signalId, siginfo_t* _info, void* _context)
		{
			FROST_UNUSED(_context);

			const char* name = "Unknown signal?";

			for (uint32_t ii = 0; ii < FROST_COUNTOF(s_signalInfo); ++ii)
			{
				const SignalInfo& signalInfo = s_signalInfo[ii];

				if (signalInfo.signalId == _signalId)
				{
					name = signalInfo.name;
					break;
				}
			}

			if (assertFunction(Location("Exception Handler", UINT32_MAX), 2
				, "%s SIGNAL %d, ERRNO %d, CODE %d"
				, name
				, _info->si_signo
				, _info->si_errno
				, _info->si_code
				) )
			{
				exit(kExitFailure, false);
			}
		}

		static constexpr uint32_t kExceptionStackSize = 64<<10;

		static char s_stack[kExceptionStackSize];

		stack_t m_oldStack;
		struct sigaction m_oldSignalAction[FROST_COUNTOF(s_signalInfo)];
	};

	char ExceptionHandler::s_stack[kExceptionStackSize];

#elif FROST_CONFIG_EXCEPTION_HANDLING_USE_WINDOWS_SEH

	struct ExceptionInfo
	{
		uint32_t exceptionCode;
		const char* name;
	};

	static const ExceptionInfo s_exceptionInfo[] =
	{   // Windows
		{ /* EXCEPTION_ACCESS_VIOLATION    */ 0xc0000005u, "Access violation."    },
		{ /* EXCEPTION_ILLEGAL_INSTRUCTION */ 0xc000001du, "Illegal instruction." },
		{ /* EXCEPTION_STACK_OVERFLOW      */ 0xc00000fdu, "Stack overflow."      },
	};

	struct ExceptionRecord
	{
		uint32_t exceptionCode;
		uint32_t exceptionFlags;

		ExceptionRecord* exceptionRecord;

		uintptr_t exceptionAddress;
		uint32_t  numberParameters;
		uintptr_t exceptionInformation[15];
	};

	struct ExceptionPointers
	{
		ExceptionRecord* exceptionRecord;
		void* contextRecord;
	};

	typedef uint32_t (__stdcall* TopLevelExceptionFilterFn)(ExceptionPointers* _exceptionInfo);
	typedef TopLevelExceptionFilterFn (__stdcall* SetUnhandledExceptionFilterFn)(TopLevelExceptionFilterFn _topLevelExceptionFilter);

#if FROST_CRT_MSVC
	template<uint16_t MaxCapacityT>
	static void wcharToNarrow(FixedStringT<MaxCapacityT>& _dst, const wchar_t* _src)
	{
		if (NULL == _src)
		{
			_dst.set("<unknown>");
			return;
		}

		typename FixedStringT<MaxCapacityT>::Pod& pod = _dst.asPod();
		const uint32_t last = MaxCapacityT - 1;
		uint32_t ii = 0;
		for (; ii < last && _src[ii] != L'\0'; ++ii)
		{
			const wchar_t wc = _src[ii];
			pod.storage[ii] = (wc >= 0x20 && wc < 0x7f) ? char(wc) : '?';
		}
		pod.storage[ii] = '\0';
		pod.len = int32_t(ii);
	}

	static void invalidParameterHandler(const wchar_t* _expression, const wchar_t* _function, const wchar_t* _file, unsigned int _line, uintptr_t /*_reserved*/)
	{
		FixedString256 expression;
		FixedString256 function;
		FixedString1024 file;
		wcharToNarrow(expression, _expression);
		wcharToNarrow(function,   _function);
		wcharToNarrow(file,       _file);

		const StringView svExpression = expression;
		const StringView svFunction   = function;
		const StringView svFile       = file;

		if (assertFunction(
			  Location("MSVC CRT Debug Error", UINT32_MAX)
			, 3
			, "CRT Invalid parameter: %S (%S:%u) %S"
			, &svFunction
			, &svFile
			, _line
			, &svExpression
			) )
		{
			exit(kExitFailure, false);
		}
	}

	static void pureCallHandler()
	{
		if (assertFunction(Location("Exception Handler", UINT32_MAX), 1, "CRT Pure virtual function call.") )
		{
			exit(kExitFailure, false);
		}
	}

#	if FROST_CONFIG_DEBUG
	static int __cdecl crtReportHook(int _reportType, char* _message, int* _returnValue)
	{
		const char* type = "Report";
		switch (_reportType)
		{
		case 0 /*_CRT_WARN  */: type = "Warning"; break;
		case 1 /*_CRT_ERROR */: type = "Error";   break;
		case 2 /*_CRT_ASSERT*/: type = "Assert";  break;
		default: break;
		}

		if (assertFunction(
			  Location("MSVC CRT Debug Error", UINT32_MAX)
			, 3
			, "CRT %s: %s"
			, type
			, NULL != _message ? _message : "<unknown>"
			) )
		{
			exit(kExitFailure, false);
		}

		if (NULL != _returnValue)
		{
			*_returnValue = 0;
		}

		return 1 /* TRUE: report handled, suppress dialog */;
	}
#	endif // FROST_CONFIG_DEBUG
#endif // FROST_CRT_MSVC

	struct ExceptionHandler
	{
		ExceptionHandler()
		{
			void* kernel32Dll = frost::dlopen("kernel32.dll");

			if (NULL != kernel32Dll)
			{
				SetUnhandledExceptionFilterFn setUnhandledExceptionFilter = frost::dlsym<SetUnhandledExceptionFilterFn>(kernel32Dll, "SetUnhandledExceptionFilter");

				if (NULL != setUnhandledExceptionFilter)
				{
					FROST_TRACE("ExceptionHandler - Windows SEH");
					setUnhandledExceptionFilter(topLevelExceptionFilter);
				}

				frost::dlclose(kernel32Dll);
			}

#if FROST_CRT_MSVC
			// Suppress Windows error mode dialogs ("application has stopped working").
			SetErrorMode(0
				| 0x0001 /* SEM_FAILCRITICALERRORS */
				| 0x0002 /* SEM_NOGPFAULTERRORBOX  */
				);

			// Suppress the "abort() has been called" message box and Windows Error Reporting.
			_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

			// Route CRT invalid parameter and pure virtual call into frost assert handler.
			_set_invalid_parameter_handler(invalidParameterHandler);
			_set_thread_local_invalid_parameter_handler(invalidParameterHandler);
			_set_purecall_handler(pureCallHandler);

#	if FROST_CONFIG_DEBUG
			// Route _CrtDbgReport "Debug Error!" dialogs through frost assert handler.
			_CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_DEBUG);
			_CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_DEBUG);
			_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
			_CrtSetReportHook(crtReportHook);
#	endif // FROST_CONFIG_DEBUG
#endif // FROST_CRT_MSVC
		}

		static uint32_t __stdcall topLevelExceptionFilter(ExceptionPointers* _info)
		{
			const char* name = "Unknown signal?";

			const uint32_t exceptionCode = _info->exceptionRecord->exceptionCode;

			for (uint32_t ii = 0; ii < FROST_COUNTOF(s_exceptionInfo); ++ii)
			{
				const ExceptionInfo& signal = s_exceptionInfo[ii];

				if (signal.exceptionCode == exceptionCode)
				{
					name = signal.name;
					break;
				}
			}

			if (assertFunction(Location("Exception Handler", UINT32_MAX), 7 /* topLevelExceptionFilter function + 6 deep stack of OS exception handler */
				, "%s Exception Code %x"
				, name
				, _info->exceptionRecord->exceptionCode
				) )
			{
				exit(kExitFailure, false);
			}

			return 0 /* EXCEPTION_CONTINUE_SEARCH */;
		}
	};

#else // Noop exception handler

	class ExceptionHandler
	{
	public:
		ExceptionHandler()
		{
			FROST_TRACE("ExceptionHandler - Noop");
		}
	};

#endif // FROST_CONFIG_EXCEPTION_HANDLING_*

	void installExceptionHandler()
	{
		static bool s_installed = false;

		if (!s_installed)
		{
			s_installed = true;

			static ExceptionHandler s_exceptionHandler;
		}
	}

	namespace
	{
		constexpr uint32_t kErrorContextBlockShift = 8;
		constexpr uint32_t kErrorContextBlockSize  = 1u << kErrorContextBlockShift;
		constexpr uint32_t kErrorContextNumBlocks  = FROST_CONFIG_ERROR_CONTEXT_RING_SIZE >> kErrorContextBlockShift;
		constexpr uint32_t kErrorContextMaxBlocks  = FROST_CONFIG_ERROR_CONTEXT_MAX_SIZE  >> kErrorContextBlockShift;
		constexpr uint32_t kErrorContextMaxFrames  = 32;

		static_assert(0 == FROST_CONFIG_ERROR_CONTEXT_RING_SIZE % kErrorContextBlockSize, "FROST_CONFIG_ERROR_CONTEXT_RING_SIZE must be a multiple of 256.");
		static_assert(0 == FROST_CONFIG_ERROR_CONTEXT_MAX_SIZE  % kErrorContextBlockSize, "FROST_CONFIG_ERROR_CONTEXT_MAX_SIZE must be a multiple of 256.");
		static_assert(kErrorContextMaxBlocks <= kErrorContextNumBlocks, "FROST_CONFIG_ERROR_CONTEXT_MAX_SIZE must not exceed FROST_CONFIG_ERROR_CONTEXT_RING_SIZE.");

		struct ErrorContextHeader
		{
			uintptr_t frames[kErrorContextMaxFrames];
			uint32_t  numFrames;
			uint32_t  numBlocks;
		};

		static_assert(sizeof(ErrorContextHeader) < kErrorContextBlockSize * kErrorContextMaxBlocks, "ErrorContextHeader leaves no room for text.");

		FROST_ALIGN_DECL_16(uint8_t s_errorContextRing[kErrorContextNumBlocks * kErrorContextBlockSize]);

		uint32_t s_errorContextHead = 1;

		uint8_t* errorContextBlock(uint32_t _block)
		{
			return &s_errorContextRing[(_block % kErrorContextNumBlocks) * kErrorContextBlockSize];
		}

		bool isErrorContextIntact(uint32_t _start)
		{
			const uint32_t head = atomicLoad(&s_errorContextHead);

			return head - _start <= kErrorContextNumBlocks;
		}

		uint32_t reserveErrorContext(uint32_t _numBlocks)
		{
			for (;;)
			{
				const uint32_t head  = atomicLoad(&s_errorContextHead);
				const uint32_t index = head % kErrorContextNumBlocks;

				const uint32_t wrapped = index + _numBlocks > kErrorContextNumBlocks
					? head + (kErrorContextNumBlocks - index)
					: head
					;
				const uint32_t start = 0 == wrapped ? kErrorContextNumBlocks : wrapped;

				if (head == atomicCompareAndSwap<uint32_t>(&s_errorContextHead, head, start + _numBlocks) )
				{
					return start;
				}
			}
		}

	} // namespace

	ErrorContext::ErrorContext(Error* _err, ErrorResult _result, const StringLiteral& _msg, int32_t _size, uint32_t _skip, const Location& _location)
		: m_err(NULL)
		, m_text(NULL)
		, m_max(0)
		, m_len(0)
		, m_start(0)
	{
		if (NULL == _err
		||  !_err->isOk() )
		{
			return;
		}

		_err->setError(_result, _msg, _location);

		const uint32_t size      = uint32_t(sizeof(ErrorContextHeader) ) + uint32_t(max(_size, 0) ) + 1;
		const uint32_t numBlocks = min( (size + kErrorContextBlockSize - 1) >> kErrorContextBlockShift, kErrorContextMaxBlocks);

		m_start = reserveErrorContext(numBlocks);

		ErrorContextHeader* header = (ErrorContextHeader*)errorContextBlock(m_start);
		header->numFrames = getCallStackFast(_skip + 1 /* skip self */, kErrorContextMaxFrames, header->frames);
		header->numBlocks = numBlocks;

		m_err   = _err;
		m_text  = (char*)(header + 1);
		m_max   = int32_t(numBlocks * kErrorContextBlockSize - sizeof(ErrorContextHeader) );
		m_text[0] = '\0';
	}

	ErrorContext::~ErrorContext()
	{
		if (NULL != m_err)
		{
			m_err->m_context = m_start;
		}
	}

	int32_t ErrorContext::write(const void* _data, int32_t _size, Error* _err)
	{
		FROST_UNUSED(_err);

		if (NULL == m_text)
		{
			return 0;
		}

		const int32_t size = clamp(_size, 0, m_max - 1 - m_len);
		memCopy(&m_text[m_len], _data, size);
		m_len += size;
		m_text[m_len] = '\0';

		return size;
	}

	int32_t write(WriterI* _writer, const Error& _error, bool _callstack, Error* _err)
	{
		const uint32_t start = _error.getContext();

		if (0 == start)
		{
			return 0;
		}

		const StringView overwritten("Error context was overwritten.\n");

		if (!isErrorContextIntact(start) )
		{
			return write(_writer, overwritten, _err);
		}

		FROST_ALIGN_DECL_16(uint8_t copy[kErrorContextMaxBlocks * kErrorContextBlockSize]);
		const ErrorContextHeader* ring = (const ErrorContextHeader*)errorContextBlock(start);
		const uint32_t numBlocks = clamp<uint32_t>(ring->numBlocks, 1, kErrorContextMaxBlocks);
		memCopy(copy, ring, numBlocks * kErrorContextBlockSize);

		if (!isErrorContextIntact(start) )
		{
			return write(_writer, overwritten, _err);
		}

		const ErrorContextHeader* header = (const ErrorContextHeader*)copy;
		const char*   text    = (const char*)(header + 1);
		const int32_t maxText = int32_t(numBlocks * kErrorContextBlockSize - sizeof(ErrorContextHeader) );

		int32_t total = write(_writer, StringView(text, strLen(text, maxText) ), _err);

		if (_callstack)
		{
			total += writeCallstack(_writer, header->frames, min(header->numFrames, kErrorContextMaxFrames), _err);
		}

		return total;
	}

} // namespace frost
