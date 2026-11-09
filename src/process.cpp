/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/process.h>

#include <stdio.h>

#ifndef FROST_CONFIG_CRT_PROCESS
#	define FROST_CONFIG_CRT_PROCESS !(0  \
			|| FROST_CRT_NONE            \
			|| FROST_PLATFORM_EMSCRIPTEN \
			|| FROST_PLATFORM_PS4        \
			|| FROST_PLATFORM_PS5        \
			|| FROST_PLATFORM_WINRT      \
			|| FROST_PLATFORM_XBOXONE    \
			)
#endif // FROST_CONFIG_CRT_PROCESS

namespace frost
{
#if FROST_CONFIG_CRT_PROCESS

#if FROST_CRT_MSVC
#	define pclose _pclose
#endif // FROST_CRT_MSVC

	static FILE* openProcess(const char* _cmd, const char* _mode)
	{
#if FROST_PLATFORM_WINDOWS
		wchar_t cmd[kMaxFilePath*2];
		wchar_t mode[4];

		if (0 == toUtf16( (uint16_t*)cmd,  FROST_COUNTOF(cmd),  _cmd)
		||  0 == toUtf16( (uint16_t*)mode, FROST_COUNTOF(mode), _mode) )
		{
			return NULL;
		}

		return _wpopen(cmd, mode);
#else
		return popen(_cmd, _mode);
#endif // FROST_PLATFORM_WINDOWS
	}

	static bool makeCommandLine(char* _out, int32_t _outSize, const StringView& _filePath, const StringView& _args)
	{
#if FROST_PLATFORM_WINDOWS
		// cmd.exe /c strips the first and the last quote of the command line.
		const StringView prefix = "\"\"";
		const StringView suffix = "\"";
#else
		const StringView prefix = "\"";
		const StringView suffix = "";
#endif // FROST_PLATFORM_WINDOWS

		const int32_t len = 0
			+ prefix.getLength()
			+ _filePath.getLength()
			+ 2
			+ _args.getLength()
			+ suffix.getLength()
			;

		if (len >= _outSize)
		{
			return false;
		}

		strCopy(_out, _outSize, prefix);
		strCat(_out, _outSize, _filePath);
		strCat(_out, _outSize, "\" ");
		strCat(_out, _outSize, _args);
		strCat(_out, _outSize, suffix);

		return true;
	}

	ProcessReader::ProcessReader()
		: m_file(NULL)
	{
	}

	ProcessReader::~ProcessReader()
	{
		FROST_ASSERT(NULL == m_file, "Process not closed!");
	}

	bool ProcessReader::open(const FilePath& _filePath, const StringView& _args, Error* _err)
	{
		FROST_ASSERT(NULL != _err, "Reader/Writer interface calling functions must handle errors.");

		if (NULL != m_file)
		{
			FROST_ERROR_SET(_err, kErrorReaderWriterAlreadyOpen, "ProcessReader: File is already open.");
			return false;
		}

		char tmp[kMaxFilePath*2];
		if (!makeCommandLine(tmp, FROST_COUNTOF(tmp), _filePath, _args) )
		{
			FROST_ERROR_SET(_err, kErrorReaderWriterOpen, "ProcessReader: Command line is too long.");
			return false;
		}

		m_file = openProcess(tmp, "r");
		if (NULL == m_file)
		{
			FROST_ERROR_SET(_err, kErrorReaderWriterOpen, "ProcessReader: Failed to open process.");
			return false;
		}

		return true;
	}

	void ProcessReader::close()
	{
		FROST_ASSERT(NULL != m_file, "Process not open!");
		FILE* file = (FILE*)m_file;
		m_exitCode = pclose(file);
		m_file = NULL;
	}

	int32_t ProcessReader::read(void* _data, int32_t _size, Error* _err)
	{
		FROST_ASSERT(NULL != _err, "Reader/Writer interface calling functions must handle errors."); FROST_UNUSED(_err);

		FILE* file = (FILE*)m_file;
		int32_t size = (int32_t)fread(_data, 1, _size, file);
		if (size != _size)
		{
			if (0 != feof(file) )
			{
				FROST_ERROR_SET(_err, kErrorReaderWriterEof, "ProcessReader: EOF.");
			}
			else if (0 != ferror(file) )
			{
				FROST_ERROR_SET(_err, kErrorReaderWriterRead, "ProcessReader: read error.");
			}

			return size >= 0 ? size : 0;
		}

		return size;
	}

	int32_t ProcessReader::getExitCode() const
	{
		return m_exitCode;
	}

	ProcessWriter::ProcessWriter()
		: m_file(NULL)
	{
	}

	ProcessWriter::~ProcessWriter()
	{
		FROST_ASSERT(NULL == m_file, "Process not closed!");
	}

	bool ProcessWriter::open(const FilePath& _filePath, const StringView& _args, Error* _err)
	{
		FROST_ASSERT(NULL != _err, "Reader/Writer interface calling functions must handle errors.");

		if (NULL != m_file)
		{
			FROST_ERROR_SET(_err, kErrorReaderWriterAlreadyOpen, "ProcessWriter: File is already open.");
			return false;
		}

		char tmp[kMaxFilePath*2];
		if (!makeCommandLine(tmp, FROST_COUNTOF(tmp), _filePath, _args) )
		{
			FROST_ERROR_SET(_err, kErrorReaderWriterOpen, "ProcessWriter: Command line is too long.");
			return false;
		}

		m_file = openProcess(tmp, "w");
		if (NULL == m_file)
		{
			FROST_ERROR_SET(_err, kErrorReaderWriterOpen, "ProcessWriter: Failed to open process.");
			return false;
		}

		return true;
	}

	void ProcessWriter::close()
	{
		FROST_ASSERT(NULL != m_file, "Process not open!");
		FILE* file = (FILE*)m_file;
		m_exitCode = pclose(file);
		m_file = NULL;
	}

	int32_t ProcessWriter::write(const void* _data, int32_t _size, Error* _err)
	{
		FROST_ASSERT(NULL != _err, "Reader/Writer interface calling functions must handle errors."); FROST_UNUSED(_err);

		FILE* file = (FILE*)m_file;
		int32_t size = (int32_t)fwrite(_data, 1, _size, file);
		if (size != _size)
		{
			if (0 != ferror(file) )
			{
				FROST_ERROR_SET(_err, kErrorReaderWriterWrite, "ProcessWriter: write error.");
			}

			return size >= 0 ? size : 0;
		}

		return size;
	}

	int32_t ProcessWriter::getExitCode() const
	{
		return m_exitCode;
	}
#endif // FROST_CONFIG_CRT_PROCESS

} // namespace frost
