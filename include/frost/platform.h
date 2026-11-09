/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_PLATFORM_H_HEADER_GUARD
#define FROST_PLATFORM_H_HEADER_GUARD

// Architecture
#define FROST_ARCH_32BIT 0
#define FROST_ARCH_64BIT 0

// Compiler
#define FROST_COMPILER_CLANG          0
#define FROST_COMPILER_CLANG_ANALYZER 0
#define FROST_COMPILER_GCC            0
#define FROST_COMPILER_MSVC           0

// Endianness
#define FROST_CPU_ENDIAN_BIG    0
#define FROST_CPU_ENDIAN_LITTLE 0

// CPU
#define FROST_CPU_ARM   0
#define FROST_CPU_JIT   0
#define FROST_CPU_MIPS  0
#define FROST_CPU_PPC   0
#define FROST_CPU_LOONGARCH64 0
#define FROST_CPU_RISCV 0
#define FROST_CPU_X86   0

// C Runtime
#define FROST_CRT_BIONIC 0
#define FROST_CRT_GLIBC  0
#define FROST_CRT_LIBCXX 0
#define FROST_CRT_MINGW  0
#define FROST_CRT_MSVC   0
#define FROST_CRT_NEWLIB 0

#ifndef FROST_CRT_NONE
#	define FROST_CRT_NONE 0
#endif // FROST_CRT_NONE

// Language standard version
#define FROST_LANGUAGE_CPP17 201703L
#define FROST_LANGUAGE_CPP20 202002L
#define FROST_LANGUAGE_CPP23 202207L

// Platform
#define FROST_PLATFORM_ANDROID    0
#define FROST_PLATFORM_BSD        0
#define FROST_PLATFORM_EMSCRIPTEN 0
#define FROST_PLATFORM_HAIKU      0
#define FROST_PLATFORM_HURD       0
#define FROST_PLATFORM_IOS        0
#define FROST_PLATFORM_LINUX      0
#define FROST_PLATFORM_NX         0
#define FROST_PLATFORM_OSX        0
#define FROST_PLATFORM_PS4        0
#define FROST_PLATFORM_PS5        0
#define FROST_PLATFORM_RPI        0
#define FROST_PLATFORM_VISIONOS   0
#define FROST_PLATFORM_WINDOWS    0
#define FROST_PLATFORM_WINRT      0
#define FROST_PLATFORM_XBOXONE    0

// http://sourceforge.net/apps/mediawiki/predef/index.php?title=Compilers
#if defined(__clang__)
// clang defines __GNUC__ or _MSC_VER
#	undef  FROST_COMPILER_CLANG
#	define FROST_COMPILER_CLANG (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#	if defined(__clang_analyzer__)
#		undef  FROST_COMPILER_CLANG_ANALYZER
#		define FROST_COMPILER_CLANG_ANALYZER 1
#	endif // defined(__clang_analyzer__)
#elif defined(_MSC_VER)
#	undef  FROST_COMPILER_MSVC
#	define FROST_COMPILER_MSVC _MSC_VER
#elif defined(__GNUC__)
#	undef  FROST_COMPILER_GCC
#	define FROST_COMPILER_GCC (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#	error "FROST_COMPILER_* is not defined!"
#endif //

// http://sourceforge.net/apps/mediawiki/predef/index.php?title=Architectures
#if defined(__arm__)     \
 || defined(__aarch64__) \
 || defined(_M_ARM)      \
 || defined(_M_ARM64)
#	undef  FROST_CPU_ARM
#	define FROST_CPU_ARM 1
#	define FROST_CACHE_LINE_SIZE 64
#elif defined(__MIPSEL__)     \
 ||   defined(__mips_isa_rev) \
 ||   defined(__mips64)
#	undef  FROST_CPU_MIPS
#	define FROST_CPU_MIPS 1
#	define FROST_CACHE_LINE_SIZE 64
#elif defined(_M_PPC)        \
 ||   defined(__powerpc__)   \
 ||   defined(__powerpc64__)
#	undef  FROST_CPU_PPC
#	define FROST_CPU_PPC 1
#	define FROST_CACHE_LINE_SIZE 128
#elif defined(__riscv)   \
 ||   defined(__riscv__) \
 ||   defined(RISCVEL)
#	undef  FROST_CPU_RISCV
#	define FROST_CPU_RISCV 1
#	define FROST_CACHE_LINE_SIZE 64
#elif defined(__loongarch__) && (__loongarch_grlen == 64)
#	undef  FROST_CPU_LOONGARCH64
#	define FROST_CPU_LOONGARCH64 1
#	define FROST_CACHE_LINE_SIZE 64
#elif defined(_M_IX86)    \
 ||   defined(_M_X64)     \
 ||   defined(__i386__)   \
 ||   defined(__x86_64__)
#	undef  FROST_CPU_X86
#	define FROST_CPU_X86 1
#	define FROST_CACHE_LINE_SIZE 64
#else // PNaCl doesn't have CPU defined.
#	undef  FROST_CPU_JIT
#	define FROST_CPU_JIT 1
#	define FROST_CACHE_LINE_SIZE 64
#endif //

#if defined(__x86_64__)    \
 || defined(_M_X64)        \
 || defined(__aarch64__)   \
 || defined(_M_ARM64)      \
 || defined(_M_ARM64EC)    \
 || defined(__64BIT__)     \
 || defined(__mips64)      \
 || defined(__powerpc64__) \
 || defined(__ppc64__)     \
 || defined(__LP64__)
#	undef  FROST_ARCH_64BIT
#	define FROST_ARCH_64BIT 64
#else
#	undef  FROST_ARCH_32BIT
#	define FROST_ARCH_32BIT 32
#endif //

#if FROST_CPU_PPC
// __BIG_ENDIAN__ is gcc predefined macro
#	if defined(__BIG_ENDIAN__)
#		undef  FROST_CPU_ENDIAN_BIG
#		define FROST_CPU_ENDIAN_BIG 1
#	else
#		undef  FROST_CPU_ENDIAN_LITTLE
#		define FROST_CPU_ENDIAN_LITTLE 1
#	endif
#else
#	undef  FROST_CPU_ENDIAN_LITTLE
#	define FROST_CPU_ENDIAN_LITTLE 1
#endif // FROST_CPU_PPC

// http://sourceforge.net/apps/mediawiki/predef/index.php?title=Operating_Systems
#if defined(_DURANGO) || defined(_XBOX_ONE)
#	undef  FROST_PLATFORM_XBOXONE
#	define FROST_PLATFORM_XBOXONE 1
#elif defined(_WIN32) || defined(_WIN64)
// http://msdn.microsoft.com/en-us/library/6sehtctf.aspx
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif // NOMINMAX
//  If _USING_V110_SDK71_ is defined it means we are using the v110_xp or v120_xp toolset.
#	if defined(_MSC_VER) && (_MSC_VER >= 1700) && !defined(_USING_V110_SDK71_)
#		include <winapifamily.h>
#	endif // defined(_MSC_VER) && (_MSC_VER >= 1700) && (!_USING_V110_SDK71_)
#	if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
#		undef  FROST_PLATFORM_WINDOWS
#		if !defined(WINVER) && !defined(_WIN32_WINNT)
//			Target Win10 and above.
#			define WINVER 0x0A00
#			define _WIN32_WINNT 0x0A00
#		endif // !defined(WINVER) && !defined(_WIN32_WINNT)
#		define FROST_PLATFORM_WINDOWS _WIN32_WINNT
#	else
#		undef  FROST_PLATFORM_WINRT
#		define FROST_PLATFORM_WINRT 1
#	endif
#elif defined(__ANDROID__)
// Android compiler defines __linux__
#	include <sys/cdefs.h> // Defines __BIONIC__ and includes android/api-level.h
#	undef  FROST_PLATFORM_ANDROID
#	define FROST_PLATFORM_ANDROID __ANDROID_API__
#elif defined(__VCCOREVER__)
// RaspberryPi compiler defines __linux__
#	undef  FROST_PLATFORM_RPI
#	define FROST_PLATFORM_RPI 1
#elif  defined(__linux__)
#	undef  FROST_PLATFORM_LINUX
#	define FROST_PLATFORM_LINUX 1
#elif  defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) \
	|| defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__)
#	undef  FROST_PLATFORM_IOS
#	define FROST_PLATFORM_IOS 1
#elif defined(__has_builtin) && __has_builtin(__is_target_os) && __is_target_os(xros)
#	undef  FROST_PLATFORM_VISIONOS
#	define FROST_PLATFORM_VISIONOS 1
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#	undef  FROST_PLATFORM_OSX
#	define FROST_PLATFORM_OSX __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#elif defined(__wasm__)
#	include <emscripten/version.h>
#	undef  FROST_PLATFORM_EMSCRIPTEN
#	define FROST_PLATFORM_EMSCRIPTEN (__EMSCRIPTEN_MAJOR__ * 10000 + __EMSCRIPTEN_MINOR__ * 100 + __EMSCRIPTEN_TINY__)
#elif defined(__ORBIS__)
#	undef  FROST_PLATFORM_PS4
#	define FROST_PLATFORM_PS4 1
#elif defined(__PROSPERO__)
#	undef  FROST_PLATFORM_PS5
#	define FROST_PLATFORM_PS5 1
#elif  defined(__FreeBSD__)        \
	|| defined(__FreeBSD_kernel__) \
	|| defined(__NetBSD__)         \
	|| defined(__OpenBSD__)        \
	|| defined(__DragonFly__)
#	undef  FROST_PLATFORM_BSD
#	define FROST_PLATFORM_BSD 1
#elif defined(__GNU__)
#	undef  FROST_PLATFORM_HURD
#	define FROST_PLATFORM_HURD 1
#elif defined(__NX__)
#	undef  FROST_PLATFORM_NX
#	define FROST_PLATFORM_NX 1
#elif defined(__HAIKU__)
#	undef  FROST_PLATFORM_HAIKU
#	define FROST_PLATFORM_HAIKU 1
#endif //

#if !FROST_CRT_NONE
// https://sourceforge.net/p/predef/wiki/Libraries/
#	if defined(__BIONIC__)
#		undef  FROST_CRT_BIONIC
#		define FROST_CRT_BIONIC 1
#	elif defined(__GLIBC__)
#		undef  FROST_CRT_GLIBC
#		define FROST_CRT_GLIBC (__GLIBC__ * 10000 + __GLIBC_MINOR__ * 100)
#	elif defined(__MINGW32__) \
	||   defined(__MINGW64__)
#		undef  FROST_CRT_MINGW
#		define FROST_CRT_MINGW 1
#	elif defined(_MSC_VER)
#		undef  FROST_CRT_MSVC
#		define FROST_CRT_MSVC 1
#	elif defined(__apple_build_version__) \
	||   FROST_PLATFORM_PS4                  \
	||   FROST_PLATFORM_EMSCRIPTEN           \
	||   defined(__llvm__)
#		undef  FROST_CRT_LIBCXX
#		define FROST_CRT_LIBCXX 1
#	endif //
#endif // !FROST_CRT_NONE

///
#define FROST_PLATFORM_POSIX (0   \
	||  FROST_PLATFORM_ANDROID    \
	||  FROST_PLATFORM_BSD        \
	||  FROST_PLATFORM_EMSCRIPTEN \
	||  FROST_PLATFORM_HAIKU      \
	||  FROST_PLATFORM_HURD       \
	||  FROST_PLATFORM_IOS        \
	||  FROST_PLATFORM_LINUX      \
	||  FROST_PLATFORM_NX         \
	||  FROST_PLATFORM_OSX        \
	||  FROST_PLATFORM_PS4        \
	||  FROST_PLATFORM_PS5        \
	||  FROST_PLATFORM_RPI        \
	||  FROST_PLATFORM_VISIONOS   \
	)

///
#define FROST_PLATFORM_NONE !(0   \
	||  FROST_PLATFORM_ANDROID    \
	||  FROST_PLATFORM_BSD        \
	||  FROST_PLATFORM_EMSCRIPTEN \
	||  FROST_PLATFORM_HAIKU      \
	||  FROST_PLATFORM_HURD       \
	||  FROST_PLATFORM_IOS        \
	||  FROST_PLATFORM_LINUX      \
	||  FROST_PLATFORM_NX         \
	||  FROST_PLATFORM_OSX        \
	||  FROST_PLATFORM_PS4        \
	||  FROST_PLATFORM_PS5        \
	||  FROST_PLATFORM_RPI        \
	||  FROST_PLATFORM_VISIONOS   \
	||  FROST_PLATFORM_WINDOWS    \
	||  FROST_PLATFORM_WINRT      \
	||  FROST_PLATFORM_XBOXONE    \
	)

///
#define FROST_PLATFORM_OS_CONSOLE  (0 \
	||  FROST_PLATFORM_NX             \
	||  FROST_PLATFORM_PS4            \
	||  FROST_PLATFORM_PS5            \
	||  FROST_PLATFORM_WINRT          \
	||  FROST_PLATFORM_XBOXONE        \
	)

///
#define FROST_PLATFORM_OS_DESKTOP  (0 \
	||  FROST_PLATFORM_BSD            \
	||  FROST_PLATFORM_HAIKU          \
	||  FROST_PLATFORM_HURD           \
	||  FROST_PLATFORM_LINUX          \
	||  FROST_PLATFORM_OSX            \
	||  FROST_PLATFORM_WINDOWS        \
	)

///
#define FROST_PLATFORM_OS_EMBEDDED (0 \
	||  FROST_PLATFORM_RPI            \
	)

///
#define FROST_PLATFORM_OS_MOBILE   (0 \
	||  FROST_PLATFORM_ANDROID        \
	||  FROST_PLATFORM_IOS            \
	)

///
#define FROST_PLATFORM_OS_WEB      (0 \
	||  FROST_PLATFORM_EMSCRIPTEN     \
	)

///
#if FROST_COMPILER_GCC
#	define FROST_COMPILER_NAME "GCC "       \
		FROST_STRINGIZE(__GNUC__) "."       \
		FROST_STRINGIZE(__GNUC_MINOR__) "." \
		FROST_STRINGIZE(__GNUC_PATCHLEVEL__)
#elif FROST_COMPILER_CLANG
#	define FROST_COMPILER_NAME "Clang "      \
		FROST_STRINGIZE(__clang_major__) "." \
		FROST_STRINGIZE(__clang_minor__) "." \
		FROST_STRINGIZE(__clang_patchlevel__)
#elif FROST_COMPILER_MSVC
#	if FROST_COMPILER_MSVC >= 1930 // Visual Studio 2022
#		define FROST_COMPILER_NAME "MSVC 17.0"
#	elif FROST_COMPILER_MSVC >= 1920 // Visual Studio 2019
#		define FROST_COMPILER_NAME "MSVC 16.0"
#	else
#		define FROST_COMPILER_NAME "MSVC"
#	endif //
#endif // FROST_COMPILER_

#if FROST_PLATFORM_ANDROID
#	define FROST_PLATFORM_NAME "Android " \
		FROST_STRINGIZE(FROST_PLATFORM_ANDROID)
#elif FROST_PLATFORM_BSD
#	define FROST_PLATFORM_NAME "BSD"
#elif FROST_PLATFORM_EMSCRIPTEN
#	define FROST_PLATFORM_NAME "Emscripten "      \
		FROST_STRINGIZE(__EMSCRIPTEN_MAJOR__) "." \
		FROST_STRINGIZE(__EMSCRIPTEN_MINOR__) "." \
		FROST_STRINGIZE(__EMSCRIPTEN_TINY__)
#elif FROST_PLATFORM_HAIKU
#	define FROST_PLATFORM_NAME "Haiku"
#elif FROST_PLATFORM_HURD
#	define FROST_PLATFORM_NAME "Hurd"
#elif FROST_PLATFORM_IOS
#	define FROST_PLATFORM_NAME "iOS"
#elif FROST_PLATFORM_LINUX
#	define FROST_PLATFORM_NAME "Linux"
#elif FROST_PLATFORM_NONE
#	define FROST_PLATFORM_NAME "None"
#elif FROST_PLATFORM_NX
#	define FROST_PLATFORM_NAME "NX"
#elif FROST_PLATFORM_OSX
#	define FROST_PLATFORM_NAME "macOS"
#elif FROST_PLATFORM_PS4
#	define FROST_PLATFORM_NAME "PlayStation 4"
#elif FROST_PLATFORM_PS5
#	define FROST_PLATFORM_NAME "PlayStation 5"
#elif FROST_PLATFORM_RPI
#	define FROST_PLATFORM_NAME "RaspberryPi"
#elif FROST_PLATFORM_VISIONOS
#	define FROST_PLATFORM_NAME "visionOS"
#elif FROST_PLATFORM_WINDOWS
#	define FROST_PLATFORM_NAME "Windows"
#elif FROST_PLATFORM_WINRT
#	define FROST_PLATFORM_NAME "WinRT"
#elif FROST_PLATFORM_XBOXONE
#	define FROST_PLATFORM_NAME "Xbox One"
#else
#	error "Unknown FROST_PLATFORM!"
#endif // FROST_PLATFORM_

#if FROST_CPU_ARM
#	define FROST_CPU_NAME "ARM"
#elif FROST_CPU_JIT
#	define FROST_CPU_NAME "JIT-VM"
#elif FROST_CPU_MIPS
#	define FROST_CPU_NAME "MIPS"
#elif FROST_CPU_PPC
#	define FROST_CPU_NAME "PowerPC"
#elif FROST_CPU_LOONGARCH64
#	define FROST_CPU_NAME "LoongArch64"
#elif FROST_CPU_RISCV
#	define FROST_CPU_NAME "RISC-V"
#elif FROST_CPU_X86
#	define FROST_CPU_NAME "x86"
#endif // FROST_CPU_

#if FROST_CRT_BIONIC
#	define FROST_CRT_NAME "Bionic libc"
#elif FROST_CRT_GLIBC
#	define FROST_CRT_NAME "GNU C Library"
#elif FROST_CRT_MSVC
#	define FROST_CRT_NAME "MSVC C Runtime"
#elif FROST_CRT_MINGW
#	define FROST_CRT_NAME "MinGW C Runtime"
#elif FROST_CRT_LIBCXX
#	define FROST_CRT_NAME "Clang C Library"
#elif FROST_CRT_NEWLIB
#	define FROST_CRT_NAME "Newlib"
#elif FROST_CRT_NONE
#	define FROST_CRT_NAME "None"
#else
#	define FROST_CRT_NAME "Unknown CRT"
#endif // FROST_CRT_

#if FROST_ARCH_32BIT
#	define FROST_ARCH_NAME "32-bit"
#elif FROST_ARCH_64BIT
#	define FROST_ARCH_NAME "64-bit"
#endif // FROST_ARCH_

#if defined(__cplusplus)
#	if FROST_COMPILER_MSVC && defined(_MSVC_LANG) && _MSVC_LANG != __cplusplus
#		error "When using MSVC you must set /Zc:__cplusplus compiler option."
#	endif // FROST_COMPILER_MSVC && defined(_MSVC_LANG) && _MSVC_LANG != __cplusplus

#	if   __cplusplus < FROST_LANGUAGE_CPP17
#		define FROST_CPP_NAME "C++Unsupported"
#	elif __cplusplus < FROST_LANGUAGE_CPP20
#		define FROST_CPP_NAME "C++17"
#	elif __cplusplus < FROST_LANGUAGE_CPP23
#		define FROST_CPP_NAME "C++20"
#	else
// See: https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b#orthodox-c
#		define FROST_CPP_NAME "C++WayTooModern"
#	endif // FROST_CPP_NAME
#else
#	define FROST_CPP_NAME "C++Unknown"
#endif // defined(__cplusplus)

#if FROST_COMPILER_MSVC && (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL)
#	error "When using MSVC you must set /Zc:preprocessor compiler option."
#endif // FROST_COMPILER_MSVC && (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL)

#if defined(__cplusplus)

static_assert(__cplusplus >= FROST_LANGUAGE_CPP20, "\n\n"
	"\t** IMPORTANT! **\n\n"
	"\tC++20 standard support is required to build.\n"
	"\t\n");

// https://releases.llvm.org/
static_assert(!FROST_COMPILER_CLANG || FROST_COMPILER_CLANG >= 110000, "\n\n"
	"\t** IMPORTANT! **\n\n"
	"\tMinimum supported Clang version is 11.0 (October 12, 2020).\n"
	"\t\n");

// https://gcc.gnu.org/releases.html
static_assert(!FROST_COMPILER_GCC || FROST_COMPILER_GCC >= 110000, "\n\n"
	"\t** IMPORTANT! **\n\n"
	"\tMinimum supported GCC version is 11.0 (April 27, 2021).\n"
	"\t\n");

// https://www.sourceware.org/glibc/wiki/Glibc%20Timeline
static_assert(!FROST_CRT_GLIBC || FROST_CRT_GLIBC >= 23100, "\n\n"
	"\t** IMPORTANT! **\n\n"
	"\tMinimum supported GLIBC version is 2.31.0 (February 1, 2020).\n"
	"\t\n");

// https://learn.microsoft.com/en-us/cpp/overview/compiler-versions
static_assert(!FROST_COMPILER_MSVC || FROST_COMPILER_MSVC >= 1935, "\n\n"
	"\t** IMPORTANT! **\n\n"
	"\tMinimum supported MSVC 19.35 / Visual Studio 2022 version 17.5 (February 21, 2023).\n"
	"\t\n");

static_assert(!FROST_CPU_ENDIAN_BIG, "\n\n"
	"\t** IMPORTANT! **\n\n"
	"\tThe code was not tested for big endian, and big endian CPU is considered unsupported.\n"
	"\t\n");

static_assert(!FROST_PLATFORM_BSD || !FROST_PLATFORM_HAIKU || !FROST_PLATFORM_HURD, "\n\n"
	"\t** IMPORTANT! **\n\n"
	"\tYou're compiling for unsupported platform!\n"
	"\tIf you wish to support this platform, make your own fork, and modify code for _yourself_.\n"
	"\t\n"
	"\tDo not submit PR to main repo, it won't be considered, and it would code rot anyway. I have no ability\n"
	"\tto test on these platforms, and over years there wasn't any serious contributor who wanted to take\n"
	"\tburden of maintaining code for these platforms.\n"
	"\t\n");

#endif // defined(__cplusplus)

#endif // FROST_PLATFORM_H_HEADER_GUARD
