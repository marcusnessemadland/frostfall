/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/guid.h>
#include <frost/file.h>
#include <frost/hash.h>

#if FROST_PLATFORM_WINDOWS
#	include <windows.h>
#	include <bcrypt.h>
#endif // FROST_PLATFORM_WINDOWS

namespace frost
{
	Guid generateGuid(Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		Guid guid;
		if (!_err->isOk() )
		{
			return guid;
		}

#if FROST_PLATFORM_WINDOWS
		if (0 > BCryptGenRandom(NULL, guid.data, sizeof(guid.data), BCRYPT_USE_SYSTEM_PREFERRED_RNG) )
		{
			FROST_ERROR_SET(_err, kErrorGuidGenerate, "Unable to generate GUID bytes.");
			return kInvalidGuid;
		}
#else
		FileReader reader;
		if (!open(&reader, "/dev/urandom", _err) )
		{
			return kInvalidGuid;
		}
		read(&reader, guid.data, sizeof(guid.data), _err);
		close(&reader);
		if (!_err->isOk() )
		{
			return kInvalidGuid;
		}
#endif // FROST_PLATFORM_WINDOWS

		guid.data[6] = (guid.data[6] & 0x0f) | 0x40;
		guid.data[8] = (guid.data[8] & 0x3f) | 0x80;
		return guid;
	}

	uint32_t hash(const Guid& _guid)
	{
		return hash<HashMurmur2A>(_guid.data, sizeof(_guid.data) );
	}

	int32_t toString(char* _out, int32_t _max, const Guid& _guid)
	{
		if (NULL == _out || 37 > _max)
		{
			if (NULL != _out && 0 < _max)
			{
				_out[0] = '\0';
			}
			return 0;
		}

		static const char s_hex[] = "0123456789abcdef";
		uint32_t pos = 0;
		for (uint32_t ii = 0; ii < sizeof(_guid.data); ++ii)
		{
			if (4 == ii || 6 == ii || 8 == ii || 10 == ii)
			{
				_out[pos++] = '-';
			}
			_out[pos++] = s_hex[_guid.data[ii] >> 4];
			_out[pos++] = s_hex[_guid.data[ii] & 0x0f];
		}
		_out[pos] = '\0';
		return int32_t(pos);
	}

	static int32_t guidHex(char _ch)
	{
		return _ch >= '0' && _ch <= '9' ? _ch - '0'
			: _ch >= 'a' && _ch <= 'f' ? _ch - 'a' + 10
			: _ch >= 'A' && _ch <= 'F' ? _ch - 'A' + 10
			: -1;
	}

	bool fromString(Guid* _out, const StringView& _str)
	{
		if (NULL == _out || 36 != _str.getLength() )
		{
			return false;
		}

		Guid guid;
		const char* str = _str.getPtr();
		uint32_t pos = 0;
		for (uint32_t ii = 0; ii < sizeof(guid.data); ++ii)
		{
			if (4 == ii || 6 == ii || 8 == ii || 10 == ii)
			{
				if ('-' != str[pos++])
				{
					return false;
				}
			}
			const int32_t high = guidHex(str[pos++]);
			const int32_t low  = guidHex(str[pos++]);
			if (0 > high || 0 > low)
			{
				return false;
			}
			guid.data[ii] = uint8_t(high << 4 | low);
		}
		*_out = guid;
		return true;
	}

} // namespace frost
