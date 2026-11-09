/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_CONFIG_H_HEADER_GUARD
#define FROST_CONFIG_H_HEADER_GUARD

#ifndef FROST_CONFIG_DEBUG
#	error "FROST_CONFIG_DEBUG must be defined in build script!"
#endif // FROST_CONFIG_DEBUG

#ifndef FROST_CONFIG_ALLOCATOR_DEBUG
#	define FROST_CONFIG_ALLOCATOR_DEBUG FROST_CONFIG_DEBUG
#endif // FROST_CONFIG_ALLOCATOR_DEBUG

#ifndef FROST_CONFIG_SUPPORTS_THREADING
#	define FROST_CONFIG_SUPPORTS_THREADING !(0 \
			|| FROST_PLATFORM_EMSCRIPTEN       \
			)
#endif // FROST_CONFIG_SUPPORTS_THREADING

/// Use fused multiply-add instructions for frost::mad and simd_f32_madd/msub/nmsub.
#ifndef FROST_CONFIG_FMA
#	if FROST_CPU_X86 \
	&& (defined(__FMA__) || (FROST_COMPILER_MSVC && defined(__AVX2__) ) )
#		define FROST_CONFIG_FMA 1
#	elif FROST_CPU_ARM               \
	&& defined(__ARM_FEATURE_FMA) \
	&& (defined(__ARM_NEON) || defined(__ARM_NEON__) )
#		define FROST_CONFIG_FMA 1
#	elif FROST_CPU_ARM && FROST_ARCH_64BIT && FROST_COMPILER_MSVC
#		define FROST_CONFIG_FMA 1
#	else
#		define FROST_CONFIG_FMA 0
#	endif
#endif // FROST_CONFIG_FMA

/// Error context ring buffer size in bytes.
#ifndef FROST_CONFIG_ERROR_CONTEXT_RING_SIZE
#	define FROST_CONFIG_ERROR_CONTEXT_RING_SIZE (64<<10)
#endif // FROST_CONFIG_ERROR_CONTEXT_RING_SIZE

/// Maximum size of one error context in bytes.
#ifndef FROST_CONFIG_ERROR_CONTEXT_MAX_SIZE
#	define FROST_CONFIG_ERROR_CONTEXT_MAX_SIZE (4<<10)
#endif // FROST_CONFIG_ERROR_CONTEXT_MAX_SIZE

#endif // FROST_CONFIG_H_HEADER_GUARD
