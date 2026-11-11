/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/asset.h>
#include <frost/readerwriter.h>

namespace frost
{
	bool readAssetHeader(const Memory* _mem, AssetHeader* _out, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		if (!_err->isOk() )
		{
			return false;
		}
		if (NULL == _mem || NULL == _mem->data || NULL == _out || kAssetHeaderSize > _mem->size)
		{
			FROST_ERROR_SET(_err, kErrorAssetHeader, "Asset header is truncated.");
			return false;
		}

		AssetHeader header;
		MemoryReader reader(_mem->data, kAssetHeaderSize);
		readHE(&reader, header.magic,            true, _err);
		readHE(&reader, header.version,          true, _err);
		readHE(&reader, header.headerSize,       true, _err);
		readHE(&reader, header.type,             true, _err);
		readHE(&reader, header.payloadVersion,   true, _err);
		read(&reader, header.guid.data, sizeof(header.guid.data), _err);
		readHE(&reader, header.payloadOffset,    true, _err);
		readHE(&reader, header.payloadSize,      true, _err);
		readHE(&reader, header.dependencyOffset, true, _err);
		readHE(&reader, header.dependencyCount,  true, _err);

		if (kAssetMagic != header.magic || kAssetVersion != header.version
		|| kAssetHeaderSize > header.headerSize || header.headerSize > _mem->size
		|| 0 == header.type || !isValid(header.guid)
		|| header.payloadOffset < header.headerSize || header.payloadOffset > _mem->size
		|| header.payloadSize > _mem->size - header.payloadOffset)
		{
			FROST_ERROR_SET(_err, kErrorAssetHeader, "Asset header or payload range is invalid.");
			return false;
		}

		if (0 != header.dependencyCount)
		{
			if (header.dependencyOffset < header.headerSize || header.dependencyOffset > _mem->size
			|| header.dependencyCount > (_mem->size - header.dependencyOffset) / kAssetDependencySize)
			{
				FROST_ERROR_SET(_err, kErrorAssetDependency, "Asset dependency range is invalid.");
				return false;
			}

			const uint32_t dependencyEnd = header.dependencyOffset + header.dependencyCount * kAssetDependencySize;
			const uint32_t payloadEnd = header.payloadOffset + header.payloadSize;
			if (header.dependencyOffset < payloadEnd && header.payloadOffset < dependencyEnd)
			{
				FROST_ERROR_SET(_err, kErrorAssetDependency, "Asset dependency table overlaps its payload.");
				return false;
			}
		}

		*_out = header;
		return _err->isOk();
	}

	bool readAssetDependency(const Memory* _mem, uint32_t _index, AssetDependency* _out, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		AssetHeader header;
		if (!readAssetHeader(_mem, &header, _err) )
		{
			return false;
		}
		if (NULL == _out || _index >= header.dependencyCount)
		{
			FROST_ERROR_SET(_err, kErrorAssetDependency, "Asset dependency index is invalid.");
			return false;
		}

		AssetDependency dependency;
		MemoryReader reader((const uint8_t*)_mem->data + header.dependencyOffset + _index * kAssetDependencySize, kAssetDependencySize);
		read(&reader, dependency.guid.data, sizeof(dependency.guid.data), _err);
		readHE(&reader, dependency.type, true, _err);
		if (!isValid(dependency.guid) || 0 == dependency.type)
		{
			FROST_ERROR_SET(_err, kErrorAssetDependency, "Asset dependency identity is invalid.");
			return false;
		}
		*_out = dependency;
		return _err->isOk();
	}

	bool writeAssetHeader(void* _data, uint32_t _size, const AssetHeader& _header, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		if (!_err->isOk() )
		{
			return false;
		}
		if (NULL == _data || kAssetHeaderSize > _size)
		{
			FROST_ERROR_SET(_err, kErrorAssetHeader, "Asset header output is too small.");
			return false;
		}

		StaticMemoryBlockWriter writer(_data, kAssetHeaderSize);
		writeLE(&writer, _header.magic,            _err);
		writeLE(&writer, _header.version,          _err);
		writeLE(&writer, _header.headerSize,       _err);
		writeLE(&writer, _header.type,             _err);
		writeLE(&writer, _header.payloadVersion,   _err);
		write(&writer, _header.guid.data, sizeof(_header.guid.data), _err);
		writeLE(&writer, _header.payloadOffset,    _err);
		writeLE(&writer, _header.payloadSize,      _err);
		writeLE(&writer, _header.dependencyOffset, _err);
		writeLE(&writer, _header.dependencyCount,  _err);
		return _err->isOk();
	}

	bool writeAssetDependency(void* _data, uint32_t _size, const AssetDependency& _dependency, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		if (!_err->isOk() )
		{
			return false;
		}
		if (NULL == _data || kAssetDependencySize > _size)
		{
			FROST_ERROR_SET(_err, kErrorAssetDependency, "Asset dependency output is too small.");
			return false;
		}

		StaticMemoryBlockWriter writer(_data, kAssetDependencySize);
		write(&writer, _dependency.guid.data, sizeof(_dependency.guid.data), _err);
		writeLE(&writer, _dependency.type, _err);
		return _err->isOk();
	}

} // namespace frost
