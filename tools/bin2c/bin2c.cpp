/*
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/allocator.h>
#include <frost/commandline.h>
#include <frost/file.h>
#include <frost/string.h>

#include <frost/debug.h>

class Bin2cWriter : public frost::WriterI
{
public:
	Bin2cWriter(frost::AllocatorI* _allocator, const frost::StringView& _name)
		: m_mb(_allocator)
		, m_mw(&m_mb)
		, m_name(_name)
		, m_outputAsCStr(false)
	{
	}

	virtual ~Bin2cWriter()
	{
	}

	virtual int32_t write(const void* _data, int32_t _size, frost::Error* _err) override
	{
		bool asCStr = true;

		const char* data = (const char*)_data;
		for (int32_t ii = 0; ii < _size && asCStr; ++ii)
		{
			char ch = data[ii];

			asCStr &= false
				|| frost::isPrint(ch)
				|| frost::isSpace(ch)
				;
		}

		m_outputAsCStr = asCStr;

		return frost::write(&m_mw, _data, _size, _err);
	}

	void output(frost::WriterI* _writer)
	{
		if (m_outputAsCStr)
		{
			outputString(_writer);
		}
		else
		{
			outputHex(_writer);
		}
	}

	void outputString(frost::WriterI* _writer)
	{
		const uint8_t* data = (const uint8_t*)m_mb.more(0);
		uint32_t size = uint32_t(frost::seek(&m_mw) );

		frost::Error err;

		frost::write(
			  _writer
			, &err
			, "static const char* %.*s = /* Generated with bin2c. */\n\t\""
			, m_name.getLength()
			, m_name.getPtr()
			);

		if (NULL != data)
		{
			bool escaped = false;

			for (uint32_t ii = 0; ii < size; ++ii)
			{
				const char ch = char(data[ii]);

				if (!escaped)
				{
					switch (ch)
					{
					case '\"': frost::write(_writer, "\\\"",        &err);  break;
					case '\n': frost::write(_writer, "\\n\"\n\t\"", &err);  break;
					case '\r': frost::write(_writer, "\\r",         &err);  break;
					case '\\': escaped = true;                 [[fallthrough]];
					default:   frost::write(_writer, ch, &err);             break;
					}
				}
				else
				{
					switch (ch)
					{
					case '\n': frost::write(_writer, "\\\"\n\t\"", &err);   break;
					case '\r':                                 [[fallthrough]];
					case '\t': frost::write(_writer, "\\", &err); [[fallthrough]];
					default  : frost::write(_writer, ch,   &err);           break;
					}

					escaped = false;
				}
			}
		}

		frost::write(_writer, &err, "\"\n\t;\n");
	}

	void outputHex(frost::WriterI* _writer)
	{
#define HEX_DUMP_WIDTH 16
#define HEX_DUMP_SPACE_WIDTH 96
#define HEX_DUMP_FORMAT "%-" FROST_STRINGIZE(HEX_DUMP_SPACE_WIDTH) "." FROST_STRINGIZE(HEX_DUMP_SPACE_WIDTH) "s"
		const uint8_t* data = (const uint8_t*)m_mb.more(0);
		uint32_t size = uint32_t(frost::seek(&m_mw) );

		frost::Error err;

		frost::write(
			  _writer
			, &err
			, "static const uint8_t %.*s[%d] = /* Generated with bin2c. */\n{\n"
			, m_name.getLength()
			, m_name.getPtr()
			, size
			);

		if (NULL != data)
		{
			char hex[HEX_DUMP_SPACE_WIDTH+1];
			char ascii[HEX_DUMP_WIDTH+1];
			uint32_t hexPos = 0;
			uint32_t asciiPos = 0;
			for (uint32_t ii = 0; ii < size; ++ii)
			{
				frost::snprintf(&hex[hexPos], sizeof(hex)-hexPos, "0x%02x, ", data[asciiPos]);
				hexPos += 6;

				ascii[asciiPos] = frost::isPrint(data[asciiPos]) && data[asciiPos] != '\\' ? data[asciiPos] : '.';
				asciiPos++;

				if (HEX_DUMP_WIDTH == asciiPos)
				{
					ascii[asciiPos] = '\0';
					frost::write(_writer, &err, "\t" HEX_DUMP_FORMAT "// %s\n", hex, ascii);
					data += asciiPos;
					hexPos   = 0;
					asciiPos = 0;
				}
			}

			if (0 != asciiPos)
			{
				ascii[asciiPos] = '\0';
				frost::write(_writer, &err, "\t" HEX_DUMP_FORMAT "// %s\n", hex, ascii);
			}
		}

		frost::write(_writer, &err, "};\n");
#undef HEX_DUMP_WIDTH
#undef HEX_DUMP_SPACE_WIDTH
#undef HEX_DUMP_FORMAT
	}

	frost::MemoryBlock  m_mb;
	frost::MemoryWriter m_mw;
	frost::StringView   m_name;
	bool             m_outputAsCStr;
};

void error(const char* _format, ...)
{
	frost::WriterI* stdOut = frost::getStdOut();
	frost::Error err;

	va_list argList;
	va_start(argList, _format);
	frost::write(stdOut, &err, "Error:\n");
	frost::write(stdOut, _format, argList, &err);
	frost::write(stdOut, &err, "\n\n");
	va_end(argList);
}

static const frost::CommandLineOption s_options[] =
{
	{ 'h',  "help",    0, NULL,          "Display this help and exit."     },
	{ 'f',  NULL,      1, "<file path>", "Input file path."                },
	{ 'o',  NULL,      1, "<file path>", "Output file path."               },
	{ 'n',  NULL,      1, "<name>",      "Array name. Defaults to 'data'." },
};

void help(const char* _error = NULL)
{
	frost::WriterI* stdOut = frost::getStdOut();
	frost::Error err;

	if (NULL != _error)
	{
		error(_error);
	}

	frost::write(stdOut, &err
		, "bin2c, binary to C\n"
		  "Copyright 2011-2026 Branimir Karadzic. All rights reserved.\n"
		  "License: See LICENSE in the project root.\n\n"
		);

	frost::write(stdOut, &err
		, "Usage: bin2c -f <in> -o <out> -n <name>\n"
		  "       bin2c <in> <out> -n <name>\n"
		  "\n"
		  "Options:\n"
		);

	frost::write(stdOut, s_options, FROST_COUNTOF(s_options), &err);

	frost::write(stdOut, &err
		, "\n"
		  "For additional information, see https://github.com/marcusnessemadland/frost\n"
		);
}

int main(int _argc, const char* _argv[])
{
	frost::DefaultAllocator allocator;
	frost::CommandLineArgs args(&allocator, _argc, _argv);

	frost::CommandLine cmdLine(args.getArgc(), args.getArgv(), s_options, FROST_COUNTOF(s_options) );

	if (cmdLine.hasArg('h', "help") )
	{
		help();
		return frost::kExitFailure;
	}

	const char* unknown = cmdLine.findUnknownOption();
	if (NULL != unknown)
	{
		char msg[256];
		frost::snprintf(msg, FROST_COUNTOF(msg), "Unknown option '%s'.", unknown);
		help(msg);
		return frost::kExitFailure;
	}

	int32_t positional = 1;

	const char* inFile = cmdLine.findOption('f');
	inFile = NULL != inFile ? inFile : cmdLine.getPositional(positional++);

	frost::FilePath filePath = NULL != inFile ? inFile : "";
	if (filePath.isEmpty() )
	{
		help("Input file name must be specified.");
		return frost::kExitFailure;
	}

	const char* outFile = cmdLine.findOption('o');
	outFile = NULL != outFile ? outFile : cmdLine.getPositional(positional++);

	frost::FilePath outFilePath = NULL != outFile ? outFile : "";
	if (outFilePath.isEmpty() )
	{
		help("Output file name must be specified.");
		return frost::kExitFailure;
	}

	frost::StringView name = cmdLine.findOption('n');
	if (name.isEmpty() )
	{
		name.set("data");
	}

	void* data = NULL;
	uint32_t size = 0;

	frost::FileReader fr;
	if (frost::open(&fr, filePath) )
	{
		size = uint32_t(frost::getSize(&fr) );

		data = frost::alloc(&allocator, size);
		frost::read(&fr, data, size, frost::ErrorAssert{});
		frost::close(&fr);

		frost::FileWriter fw;
		if (frost::open(&fw, outFilePath) )
		{
			Bin2cWriter writer(&allocator, name);
			frost::write(&writer, data, size, frost::ErrorAssert{});

			writer.output(&fw);
			frost::close(&fw);
		}
		else
		{
			frost::StringView path = outFilePath;
			error("Failed to open output file '%.*s'.\n", path.getLength(), path.getPtr() );
		}

		frost::free(&allocator, data);
	}
	else
	{
		frost::StringView path = filePath;
		error("Failed to open input file '%.*s'.\n", path.getLength(), path.getPtr() );
	}

	return 0;
}
