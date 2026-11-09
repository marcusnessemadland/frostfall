/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_PROCESS_H_HEADER_GUARD
#define FROST_PROCESS_H_HEADER_GUARD

#include "readerwriter.h"

namespace frost
{
	///
	class ProcessReader
		: public ProcessOpenI
		, public CloserI
		, public ReaderI
	{
	public:
		///
		ProcessReader();

		///
		~ProcessReader();

		///
		virtual bool open(const FilePath& _filePath, const StringView& _args, Error* _err) override;

		///
		virtual void close() override;

		///
		virtual int32_t read(void* _data, int32_t _size, Error* _err) override;

		///
		int32_t getExitCode() const;

	private:
		void* m_file;
		int32_t m_exitCode;
	};

	///
	class ProcessWriter
		: public ProcessOpenI
		, public CloserI
		, public WriterI
	{
	public:
		///
		ProcessWriter();

		///
		~ProcessWriter();

		///
		virtual bool open(const FilePath& _filePath, const StringView& _args, Error* _err) override;

		///
		virtual void close() override;

		///
		virtual int32_t write(const void* _data, int32_t _size, Error* _err) override;

		///
		int32_t getExitCode() const;

	private:
		void* m_file;
		int32_t m_exitCode;
	};

} // namespace frost

#endif // FROST_PROCESS_H_HEADER_GUARD
