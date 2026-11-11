/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_ASSET_H_HEADER_GUARD
#define FROST_ASSET_H_HEADER_GUARD

#include "guid.h"
#include "memory.h"

namespace frost
{
	FROST_ERROR_RESULT(kErrorAssetHeader,     FROST_MAKEFOURCC('a', 's', 'h', 'd') );
	FROST_ERROR_RESULT(kErrorAssetDependency, FROST_MAKEFOURCC('a', 's', 'd', 'p') );

	constexpr uint32_t kAssetMagic          = FROST_MAKEFOURCC('F', 'R', 'S', 'T');
	constexpr uint16_t kAssetVersion        = 1;
	constexpr uint16_t kAssetHeaderSize     = 48;
	constexpr uint32_t kAssetDependencySize = 20;

	/// Common asset envelope. Integers on disk are little endian, GUIDs are bytes.
	/// All offsets are relative to the beginning of the asset. Header, dependency
	/// table, and payload occupy disjoint ranges. Never write this struct directly.
	struct AssetHeader
	{
		uint32_t magic            = kAssetMagic;
		uint16_t version          = kAssetVersion;
		uint16_t headerSize       = kAssetHeaderSize;
		uint32_t type             = 0;
		uint32_t payloadVersion   = 0;
		Guid     guid;
		uint32_t payloadOffset    = kAssetHeaderSize;
		uint32_t payloadSize      = 0;
		uint32_t dependencyOffset = kAssetHeaderSize;
		uint32_t dependencyCount  = 0;
	};

	/// Persistent dependency identity. Runtime resources use resolved typed handles.
	struct AssetDependency
	{
		Guid     guid;
		uint32_t type = 0;
	};

	/// Read and validate the envelope and its ranges. Does not consume _mem.
	bool readAssetHeader(const Memory* _mem, AssetHeader* _out, Error* _err = ErrorIgnore{});

	/// Read a dependency from a validated asset. Does not consume _mem.
	bool readAssetDependency(const Memory* _mem, uint32_t _index, AssetDependency* _out, Error* _err = ErrorIgnore{});

	/// Encode an envelope into at least kAssetHeaderSize writable bytes.
	bool writeAssetHeader(void* _data, uint32_t _size, const AssetHeader& _header, Error* _err = ErrorIgnore{});

	/// Encode a dependency into at least kAssetDependencySize writable bytes.
	bool writeAssetDependency(void* _data, uint32_t _size, const AssetDependency& _dependency, Error* _err = ErrorIgnore{});

} // namespace frost

#endif // FROST_ASSET_H_HEADER_GUARD
