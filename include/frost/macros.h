/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_H_HEADER_GUARD
#	error "Do not include macros.h directly #include <frost/frost.h> instead."
#endif // FROST_H_HEADER_GUARD

#ifndef FROST_MACROS_H_HEADER_GUARD
#define FROST_MACROS_H_HEADER_GUARD

#define FROST_VA_ARGS_COUNT(...) FROST_VA_ARGS_COUNT_(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define FROST_VA_ARGS_COUNT_(_a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8, _a9, _a10, _a11, _a12, _a13, _a14, _a15, _a16, _last, ...) _last

///
#define FROST_MACRO_DISPATCHER(_func, ...) FROST_MACRO_DISPATCHER_1_(_func, FROST_VA_ARGS_COUNT(__VA_ARGS__) )
#define FROST_MACRO_DISPATCHER_1_(_func, _argCount) FROST_MACRO_DISPATCHER_2_(_func, _argCount)
#define FROST_MACRO_DISPATCHER_2_(_func, _argCount) FROST_CONCATENATE(_func, _argCount)

///
#define FROST_MAKEFOURCC(_a, _b, _c, _d) ( ( (uint32_t)(_a) | ( (uint32_t)(_b) << 8) | ( (uint32_t)(_c) << 16) | ( (uint32_t)(_d) << 24) ) )

///
#define FROST_STRINGIZE(_x) FROST_STRINGIZE_(_x)
#define FROST_STRINGIZE_(_x) #_x

///
#define FROST_CONCATENATE(_x, _y) FROST_CONCATENATE_(_x, _y)
#define FROST_CONCATENATE_(_x, _y) _x ## _y

///
#define FROST_FILE_LINE_LITERAL "" __FILE__ "(" FROST_STRINGIZE(__LINE__) "): "

///
#define FROST_ALIGNOF(_type) __alignof(_type)

#if defined(__has_feature)
#	define FROST_CLANG_HAS_FEATURE(_x) __has_feature(_x)
#else
#	define FROST_CLANG_HAS_FEATURE(_x) 0
#endif // defined(__has_feature)

#if defined(__has_extension)
#	define FROST_CLANG_HAS_EXTENSION(_x) __has_extension(_x)
#else
#	define FROST_CLANG_HAS_EXTENSION(_x) 0
#endif // defined(__has_extension)

#if FROST_COMPILER_GCC || FROST_COMPILER_CLANG
#	define FROST_ASSUME(_condition) FROST_MACRO_BLOCK_BEGIN if (!(_condition) ) { __builtin_unreachable(); } FROST_MACRO_BLOCK_END
#	define FROST_ALIGN_DECL(_align, _decl) _decl __attribute__( (aligned(_align) ) )
#	define FROST_FORCE_INLINE inline __attribute__( (__always_inline__) )
#	define FROST_FUNCTION __PRETTY_FUNCTION__
#	define FROST_LIKELY(_x)   __builtin_expect(!!(_x), 1)
#	define FROST_UNLIKELY(_x) __builtin_expect(!!(_x), 0)
#	define FROST_NO_INLINE   __attribute__( (noinline) )
#	define FROST_CONST_FUNC  __attribute__( (pure) )
#	define FROST_UNREACHABLE __builtin_unreachable()
#	define FROST_NO_VTABLE
#	define FROST_PRINTF_ARGS(_format, _args) __attribute__( (format(__printf__, _format, _args) ) )
#	define FROST_THREAD_LOCAL __thread
#	define FROST_ATTRIBUTE(_x) __attribute__( (_x) )
#	define FROST_ATTRIBUTE_INTRINSIC FROST_FORCE_INLINE
#	define FROST_STACK_ALLOC(_size) __builtin_alloca(_size)

#	if FROST_CRT_MSVC
#		define __stdcall
#	endif // FROST_CRT_MSVC

#elif FROST_COMPILER_MSVC
#	define FROST_ASSUME(_condition) __assume(_condition)
#	define FROST_ALIGN_DECL(_align, _decl) __declspec(align(_align) ) _decl
#	define FROST_FORCE_INLINE __forceinline
#	define FROST_FUNCTION __FUNCTION__
#	define FROST_LIKELY(_x)   (_x)
#	define FROST_UNLIKELY(_x) (_x)
#	define FROST_NO_INLINE __declspec(noinline)
#	define FROST_CONST_FUNC  __declspec(noalias)
#	define FROST_UNREACHABLE __assume(false)
#	define FROST_NO_VTABLE __declspec(novtable)
#	define FROST_PRINTF_ARGS(_format, _args)
#	define FROST_THREAD_LOCAL __declspec(thread)
#	define FROST_ATTRIBUTE(_x)
#	define FROST_ATTRIBUTE_INTRINSIC [[msvc::intrinsic]]

extern "C" void* __cdecl _alloca(size_t _size);
#	define FROST_STACK_ALLOC(_size) ::_alloca(_size)
#else
#	error "Unknown FROST_COMPILER_?"
#endif

#if FROST_COMPILER_CLANG
#	define FROST_REQUIRE_CONSTANT(_x) FROST_ATTRIBUTE(__diagnose_if__(!__builtin_constant_p(_x), #_x " must be constant!", "error") )
#else
#	define FROST_REQUIRE_CONSTANT(_x)
#endif // if FROST_COMPILER_CLANG

/// The return value of the function is solely a function of the arguments.
///
#define FROST_CONSTEXPR_FUNC constexpr FROST_CONST_FUNC

///
#define FROST_ALIGN_DECL_16(_decl) FROST_ALIGN_DECL(16, _decl)
#define FROST_ALIGN_DECL_256(_decl) FROST_ALIGN_DECL(256, _decl)
#define FROST_ALIGN_DECL_CACHE_LINE(_decl) FROST_ALIGN_DECL(FROST_CACHE_LINE_SIZE, _decl)

///
#define FROST_MACRO_BLOCK_BEGIN do {
#define FROST_MACRO_BLOCK_END } while (false)
#define FROST_NOOP(...) FROST_MACRO_BLOCK_BEGIN FROST_MACRO_BLOCK_END

///
#define FROST_UNUSED_1(_a1)                                              \
	FROST_MACRO_BLOCK_BEGIN                                              \
		FROST_PRAGMA_DIAGNOSTIC_PUSH();                                  \
		/*FROST_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wuseless-cast");*/ \
		(void)(true ? (void)0 : ( (void)(_a1) ) );                    \
		FROST_PRAGMA_DIAGNOSTIC_POP();                                   \
	FROST_MACRO_BLOCK_END

#define FROST_UNUSED_2(_a1, _a2) FROST_UNUSED_1(_a1); FROST_UNUSED_1(_a2)
#define FROST_UNUSED_3(_a1, _a2, _a3) FROST_UNUSED_2(_a1, _a2); FROST_UNUSED_1(_a3)
#define FROST_UNUSED_4(_a1, _a2, _a3, _a4) FROST_UNUSED_3(_a1, _a2, _a3); FROST_UNUSED_1(_a4)
#define FROST_UNUSED_5(_a1, _a2, _a3, _a4, _a5) FROST_UNUSED_4(_a1, _a2, _a3, _a4); FROST_UNUSED_1(_a5)
#define FROST_UNUSED_6(_a1, _a2, _a3, _a4, _a5, _a6) FROST_UNUSED_5(_a1, _a2, _a3, _a4, _a5); FROST_UNUSED_1(_a6)
#define FROST_UNUSED_7(_a1, _a2, _a3, _a4, _a5, _a6, _a7) FROST_UNUSED_6(_a1, _a2, _a3, _a4, _a5, _a6); FROST_UNUSED_1(_a7)
#define FROST_UNUSED_8(_a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8) FROST_UNUSED_7(_a1, _a2, _a3, _a4, _a5, _a6, _a7); FROST_UNUSED_1(_a8)
#define FROST_UNUSED_9(_a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8, _a9) FROST_UNUSED_8(_a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8); FROST_UNUSED_1(_a9)
#define FROST_UNUSED_10(_a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8, _a9, _a10) FROST_UNUSED_9(_a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8, _a9); FROST_UNUSED_1(_a10)
#define FROST_UNUSED_11(_a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8, _a9, _a10, _a11) FROST_UNUSED_10(_a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8, _a9, _a10); FROST_UNUSED_1(_a11)
#define FROST_UNUSED_12(_a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8, _a9, _a10, _a11, _a12) FROST_UNUSED_11(_a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8, _a9, _a10, _a11); FROST_UNUSED_1(_a12)

#define FROST_UNUSED(...) FROST_MACRO_DISPATCHER(FROST_UNUSED_, __VA_ARGS__)(__VA_ARGS__)

///
#if FROST_COMPILER_CLANG
#	define FROST_PRAGMA_DIAGNOSTIC_PUSH_CLANG_()     _Pragma("clang diagnostic push")
#	define FROST_PRAGMA_DIAGNOSTIC_POP_CLANG_()      _Pragma("clang diagnostic pop")
#	define FROST_PRAGMA_DIAGNOSTIC_IGNORED_CLANG(_x) _Pragma(FROST_STRINGIZE(clang diagnostic ignored _x) )
#else
#	define FROST_PRAGMA_DIAGNOSTIC_PUSH_CLANG_()
#	define FROST_PRAGMA_DIAGNOSTIC_POP_CLANG_()
#	define FROST_PRAGMA_DIAGNOSTIC_IGNORED_CLANG(_x)
#endif // FROST_COMPILER_CLANG

#if FROST_COMPILER_GCC
#	define FROST_PRAGMA_DIAGNOSTIC_PUSH_GCC_()       _Pragma("GCC diagnostic push")
#	define FROST_PRAGMA_DIAGNOSTIC_POP_GCC_()        _Pragma("GCC diagnostic pop")
#	define FROST_PRAGMA_DIAGNOSTIC_IGNORED_GCC(_x)   _Pragma(FROST_STRINGIZE(GCC diagnostic ignored _x) )
#else
#	define FROST_PRAGMA_DIAGNOSTIC_PUSH_GCC_()
#	define FROST_PRAGMA_DIAGNOSTIC_POP_GCC_()
#	define FROST_PRAGMA_DIAGNOSTIC_IGNORED_GCC(_x)
#endif // FROST_COMPILER_GCC

#if FROST_COMPILER_MSVC
#	define FROST_PRAGMA_DIAGNOSTIC_PUSH_MSVC_()     __pragma(warning(push) )
#	define FROST_PRAGMA_DIAGNOSTIC_POP_MSVC_()      __pragma(warning(pop) )
#	define FROST_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(_x) __pragma(warning(disable:_x) )
#else
#	define FROST_PRAGMA_DIAGNOSTIC_PUSH_MSVC_()
#	define FROST_PRAGMA_DIAGNOSTIC_POP_MSVC_()
#	define FROST_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(_x)
#endif // FROST_COMPILER_CLANG

#if FROST_COMPILER_CLANG
#	define FROST_PRAGMA_DIAGNOSTIC_PUSH              FROST_PRAGMA_DIAGNOSTIC_PUSH_CLANG_
#	define FROST_PRAGMA_DIAGNOSTIC_POP               FROST_PRAGMA_DIAGNOSTIC_POP_CLANG_
#	define FROST_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC FROST_PRAGMA_DIAGNOSTIC_IGNORED_CLANG
#elif FROST_COMPILER_GCC
#	define FROST_PRAGMA_DIAGNOSTIC_PUSH              FROST_PRAGMA_DIAGNOSTIC_PUSH_GCC_
#	define FROST_PRAGMA_DIAGNOSTIC_POP               FROST_PRAGMA_DIAGNOSTIC_POP_GCC_
#	define FROST_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC FROST_PRAGMA_DIAGNOSTIC_IGNORED_GCC
#elif FROST_COMPILER_MSVC
#	define FROST_PRAGMA_DIAGNOSTIC_PUSH              FROST_PRAGMA_DIAGNOSTIC_PUSH_MSVC_
#	define FROST_PRAGMA_DIAGNOSTIC_POP               FROST_PRAGMA_DIAGNOSTIC_POP_MSVC_
#	define FROST_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC(_x)
#endif // FROST_COMPILER_

///
#if FROST_COMPILER_MSVC
#	define FROST_FP_CONTRACT_OFF_BEGIN()
#	define FROST_FP_CONTRACT_OFF_END()
#	define FROST_FP_PRECISE_BEGIN()      __pragma(float_control(push) ) __pragma(float_control(precise, on) ) __pragma(fp_contract(off) )
#	define FROST_FP_PRECISE_END()        __pragma(float_control(pop) )
#elif FROST_COMPILER_CLANG && (FROST_CPU_X86 || (FROST_CPU_ARM && FROST_ARCH_64BIT) )
#	define FROST_FP_CONTRACT_OFF_BEGIN() _Pragma("float_control(push)") _Pragma("clang fp contract(off)")
#	define FROST_FP_CONTRACT_OFF_END()   _Pragma("float_control(pop)")
#	define FROST_FP_PRECISE_BEGIN()      _Pragma("float_control(push)") _Pragma("float_control(precise, on)") _Pragma("clang fp contract(off)")
#	define FROST_FP_PRECISE_END()        _Pragma("float_control(pop)")
#elif FROST_COMPILER_CLANG
#	define FROST_FP_CONTRACT_OFF_BEGIN() _Pragma("clang fp contract(off)")
#	define FROST_FP_CONTRACT_OFF_END()
#	define FROST_FP_PRECISE_BEGIN()      _Pragma("clang fp contract(off)")
#	define FROST_FP_PRECISE_END()
#elif FROST_COMPILER_GCC
#	define FROST_FP_CONTRACT_OFF_BEGIN()
#	define FROST_FP_CONTRACT_OFF_END()
#	define FROST_FP_PRECISE_BEGIN()      _Pragma("GCC push_options") _Pragma("GCC optimize(\"no-fast-math\",\"fp-contract=off\")")
#	define FROST_FP_PRECISE_END()        _Pragma("GCC pop_options")
#else
#	define FROST_FP_CONTRACT_OFF_BEGIN()
#	define FROST_FP_CONTRACT_OFF_END()
#	define FROST_FP_PRECISE_BEGIN()
#	define FROST_FP_PRECISE_END()
#endif // FROST_COMPILER_

/// No default constructor.
#define FROST_CLASS_NO_DEFAULT_CTOR(_class) \
	_class() = delete

/// No copy constructor.
#define FROST_CLASS_NO_COPY_CTOR(_class) \
	_class(const _class& _rhs) = delete

/// No copy assignment operator.
#define FROST_CLASS_NO_COPY_ASSIGNMENT(_class) \
	_class& operator=(const _class& _rhs) = delete

/// No copy construcor, and copy assignment operator.
#define FROST_CLASS_NO_COPY(_class)   \
	FROST_CLASS_NO_COPY_CTOR(_class); \
	FROST_CLASS_NO_COPY_ASSIGNMENT(_class)

///
#define FROST_CLASS_ALLOCATOR(_class)              \
	public: void* operator new(size_t _size);   \
	public: void  operator delete(void* _ptr);  \
	public: void* operator new[](size_t _size); \
	public: void  operator delete[](void* _ptr)

#define FROST_CLASS_1(_class, _a1) FROST_CONCATENATE(FROST_CLASS_, _a1)(_class)
#define FROST_CLASS_2(_class, _a1, _a2) FROST_CLASS_1(_class, _a1); FROST_CLASS_1(_class, _a2)
#define FROST_CLASS_3(_class, _a1, _a2, _a3) FROST_CLASS_2(_class, _a1, _a2); FROST_CLASS_1(_class, _a3)
#define FROST_CLASS_4(_class, _a1, _a2, _a3, _a4) FROST_CLASS_3(_class, _a1, _a2, _a3); FROST_CLASS_1(_class, _a4)
#define FROST_CLASS(_class, ...) FROST_MACRO_DISPATCHER(FROST_CLASS_, __VA_ARGS__)(_class, __VA_ARGS__)

#ifndef FROST_ASSERT
#	if FROST_CONFIG_DEBUG
#		define FROST_ASSERT _FROST_ASSERT
#	else
#		define FROST_ASSERT(...) FROST_NOOP()
#	endif // FROST_CONFIG_DEBUG
#endif // FROST_ASSERT

#ifndef FROST_ASSERT_LOC
#	if FROST_CONFIG_DEBUG
#		define FROST_ASSERT_LOC _FROST_ASSERT_LOC
#	else
#		define FROST_ASSERT_LOC(_location, ...) FROST_MACRO_BLOCK_BEGIN FROST_UNUSED(_location); FROST_MACRO_BLOCK_END
#	endif // FROST_CONFIG_DEBUG
#endif // FROST_ASSERT_LOC

#ifndef FROST_TRACE
#	if FROST_CONFIG_DEBUG
#		define FROST_TRACE _FROST_TRACE
#	else
#		define FROST_TRACE(...) FROST_NOOP()
#	endif // FROST_CONFIG_DEBUG
#endif // FROST_TRACE

#ifndef FROST_TRACE_LOC
#	if FROST_CONFIG_DEBUG
#		define FROST_TRACE_LOC _FROST_TRACE_LOC
#	else
#		define FROST_TRACE_LOC(_location, ...) FROST_MACRO_BLOCK_BEGIN FROST_UNUSED(_location); FROST_MACRO_BLOCK_END
#	endif // FROST_CONFIG_DEBUG
#endif // FROST_TRACE_LOC

#ifndef FROST_WARN
#	if FROST_CONFIG_DEBUG
#		define FROST_WARN _FROST_WARN
#	else
#		define FROST_WARN(...) FROST_NOOP()
#	endif // FROST_CONFIG_DEBUG
#endif // FROST_ASSERT

#ifndef FROST_WARN_LOC
#	if FROST_CONFIG_DEBUG
#		define FROST_WARN_LOC _FROST_WARN_LOC
#	else
#		define FROST_WARN_LOC(_location, ...) FROST_MACRO_BLOCK_BEGIN FROST_UNUSED(_location); FROST_MACRO_BLOCK_END
#	endif // FROST_CONFIG_DEBUG
#endif // FROST_WARN_LOC

#define _FROST_TRACE(_format, ...)                                                                    \
	FROST_MACRO_BLOCK_BEGIN                                                                           \
		frost::debugPrintf(__FILE__ "(" FROST_STRINGIZE(__LINE__) "): FROST " _format "\n" __VA_OPT__(,) __VA_ARGS__); \
	FROST_MACRO_BLOCK_END

#define _FROST_TRACE_LOC(_location, _format, ...)                                                          \
	FROST_MACRO_BLOCK_BEGIN                                                                                \
		frost::debugPrintf("%s(%d): FROST " _format "\n", _location.filePath, _location.line __VA_OPT__(,) __VA_ARGS__); \
	FROST_MACRO_BLOCK_END

#define _FROST_ASSERT(_condition, _format, ...)                                                                      \
	FROST_MACRO_BLOCK_BEGIN                                                                                          \
		if (!FROST_IGNORE_C4127(_condition)                                                                          \
		&&  frost::assertFunction(frost::Location::current(), 0, "ASSERT %s -> " _format, #_condition __VA_OPT__(,) __VA_ARGS__) ) \
		{                                                                                                         \
			frost::debugBreak();                                                                                     \
		}                                                                                                         \
	FROST_MACRO_BLOCK_END

#define _FROST_ASSERT_LOC(_location, _condition, _format, ...)                                          \
	FROST_MACRO_BLOCK_BEGIN                                                                             \
		if  (!FROST_IGNORE_C4127(_condition)                                                            \
		&&   frost::assertFunction(_location, 0, "ASSERT %s -> " _format, #_condition __VA_OPT__(,) __VA_ARGS__) ) \
		{                                                                                            \
			frost::debugBreak();                                                                        \
		}                                                                                            \
	FROST_MACRO_BLOCK_END

#define _FROST_WARN(_condition, _format, ...)            \
	FROST_MACRO_BLOCK_BEGIN                              \
		if (!FROST_IGNORE_C4127(_condition) )            \
		{                                             \
			FROST_TRACE("WARN " _format __VA_OPT__(,) __VA_ARGS__); \
		}                                             \
	FROST_MACRO_BLOCK_END

#define _FROST_WARN_LOC(_location, _condition, _format, ...)             \
	FROST_MACRO_BLOCK_BEGIN                                              \
		if (!FROST_IGNORE_C4127(_condition) )                            \
		{                                                             \
			_FROST_TRACE_LOC(_location, "WARN " _format __VA_OPT__(,) __VA_ARGS__); \
		}                                                             \
	FROST_MACRO_BLOCK_END

#if FROST_CLANG_HAS_FEATURE(thread_sanitizer) || defined(__SANITIZE_THREAD__)
#	define FROST_SANITIZER_THREAD 1
#else
#	define FROST_SANITIZER_THREAD 0
#endif // FROST_CLANG_HAS_FEATURE(thread_sanitizer)

#if FROST_CLANG_HAS_FEATURE(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#	define FROST_SANITIZER_ADDRESS 1
#else
#	define FROST_SANITIZER_ADDRESS 0
#endif // FROST_CLANG_HAS_FEATURE(address_sanitizer)

#if FROST_SANITIZER_THREAD
extern "C"
{
	void __tsan_mutex_pre_lock(void* _addr, unsigned _flags);
	void __tsan_mutex_post_lock(void* _addr, unsigned _flags, int _recursion);
	int  __tsan_mutex_pre_unlock(void* _addr, unsigned _flags);
	void __tsan_mutex_post_unlock(void* _addr, unsigned _flags);

	void* __tsan_get_current_fiber();
	void* __tsan_create_fiber(unsigned _flags);
	void  __tsan_destroy_fiber(void* _fiber);
	void  __tsan_switch_to_fiber(void* _fiber, unsigned _flags);
}

/// Annotations of custom mutex for ThreadSanitizer.
#	define FROST_TSAN_MUTEX_PRE_LOCK(_ptr)    __tsan_mutex_pre_lock( (void*)(_ptr), 0)
#	define FROST_TSAN_MUTEX_POST_LOCK(_ptr)   __tsan_mutex_post_lock( (void*)(_ptr), 0, 0)
#	define FROST_TSAN_MUTEX_PRE_UNLOCK(_ptr)  __tsan_mutex_pre_unlock( (void*)(_ptr), 0)
#	define FROST_TSAN_MUTEX_POST_UNLOCK(_ptr) __tsan_mutex_post_unlock( (void*)(_ptr), 0)

/// Annotations of fiber switch for ThreadSanitizer.
#	define FROST_TSAN_FIBER_CURRENT()         __tsan_get_current_fiber()
#	define FROST_TSAN_FIBER_CREATE()          __tsan_create_fiber(0)
#	define FROST_TSAN_FIBER_DESTROY(_fiber)   __tsan_destroy_fiber(_fiber)
#	define FROST_TSAN_FIBER_SWITCH(_fiber)    __tsan_switch_to_fiber(_fiber, 0)
#else
#	define FROST_TSAN_MUTEX_PRE_LOCK(_ptr)    FROST_NOOP(_ptr)
#	define FROST_TSAN_MUTEX_POST_LOCK(_ptr)   FROST_NOOP(_ptr)
#	define FROST_TSAN_MUTEX_PRE_UNLOCK(_ptr)  FROST_NOOP(_ptr)
#	define FROST_TSAN_MUTEX_POST_UNLOCK(_ptr) FROST_NOOP(_ptr)
#	define FROST_TSAN_FIBER_CURRENT()         NULL
#	define FROST_TSAN_FIBER_CREATE()          NULL
#	define FROST_TSAN_FIBER_DESTROY(_fiber)   FROST_NOOP(_fiber)
#	define FROST_TSAN_FIBER_SWITCH(_fiber)    FROST_NOOP(_fiber)
#endif // FROST_SANITIZER_THREAD

#if FROST_SANITIZER_ADDRESS
extern "C"
{
	void __sanitizer_start_switch_fiber(void** _fakeStackSave, const void* _bottom, size_t _size);
	void __sanitizer_finish_switch_fiber(void* _fakeStackSave, const void** _bottomOld, size_t* _sizeOld);
}

/// Annotations of fiber switch for AddressSanitizer.
#	define FROST_ASAN_FIBER_SWITCH_START(_fakeStackSave, _bottom, _size)    __sanitizer_start_switch_fiber(_fakeStackSave, _bottom, _size)
#	define FROST_ASAN_FIBER_SWITCH_FINISH(_fakeStack, _outBottom, _outSize) __sanitizer_finish_switch_fiber(_fakeStack, _outBottom, _outSize)
#else
#	define FROST_ASAN_FIBER_SWITCH_START(_fakeStackSave, _bottom, _size)    FROST_NOOP(_fakeStackSave, _bottom, _size)
#	define FROST_ASAN_FIBER_SWITCH_FINISH(_fakeStack, _outBottom, _outSize) FROST_NOOP(_fakeStack, _outBottom, _outSize)
#endif // FROST_SANITIZER_ADDRESS

// static_assert sometimes causes unused-local-typedef...
FROST_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wunused-local-typedef")

#endif // FROST_MACROS_H_HEADER_GUARD
