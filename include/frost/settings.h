/*
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_SETTINGS_H_HEADER_GUARD
#define FROST_SETTINGS_H_HEADER_GUARD

#include "allocator.h"
#include "readerwriter.h"
#include "string.h"

namespace frost
{
	///
	class Settings
	{
	public:
		///
		Settings(AllocatorI* _allocator, const void* _data = NULL, uint32_t _len = 0);

		///
		~Settings();

		///
		void clear();

		///
		void load(const void* _data, uint32_t _len);

		///
		StringView get(const StringView& _name) const;

		///
		void set(const StringView& _name, const StringView& _value = "");

		///
		void remove(const StringView& _name) const;

		///
		int32_t read(ReaderSeekerI* _reader, Error* _err);

		///
		int32_t write(WriterI* _writer, Error* _err) const;

	private:
		Settings();

		struct Ini;

		AllocatorI* m_allocator;
		Ini*        m_ini;
	};

	///
	int32_t read(ReaderSeekerI* _reader, Settings& _settings, Error* _err);

	///
	int32_t write(WriterI* _writer, const Settings& _settings, Error* _err);

} // namespace frost

#endif // FROST_SETTINGS_H_HEADER_GUARD
