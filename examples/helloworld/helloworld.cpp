/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

// Engine
#include <frost/ecs.h>
#include <frost/file.h>
#include <frost/commandline.h>

// Plugins
#include <device.h>
#include <graphics.h>

//#include "camera.h"

static const frost::CommandLineOption s_options[] =
{
	{ 'h',  "help",   0, NULL,       "Display this help and exit." },
	{ NULL,  "width",  1, "<width>",  "Startup window width.\n"     },
	{ NULL,  "height", 1, "<height>", "Startup window height.\n"    }
};

static void help(const char* _error = NULL)
{
	if (NULL != _error)
	{
		frost::printf("Error:\n%s\n\n", _error);
	}

	frost::printf("helloworld, frost example.\n");

	frost::Error err;
	frost::write(frost::getStdOut(), s_options, FROST_COUNTOF(s_options), &err);
}

int main(int _argc, const char* _argv[])
{
	frost::DefaultAllocator allocator;

	// Parse commands
	frost::CommandLineArgs args(&allocator, _argc, _argv);
	frost::CommandLine cmdLine(args.getArgc(), args.getArgv(), s_options, FROST_COUNTOF(s_options));

	if (cmdLine.hasArg('h', "help") )
	{
		help();
		return frost::kExitFailure;
	}

	uint32_t width = 1280;
	const char* widthArg = cmdLine.findOption("width");
	if (NULL != widthArg)
	{
		frost::fromString(&width, widthArg);
	}

	uint32_t height = 720;
	const char* heightArg = cmdLine.findOption("height");
	if (NULL != heightArg)
	{
		frost::fromString(&height, heightArg);
	}

	// Run app...
	frost::App app(&allocator);
	{
		app.insertResource<frost::DeviceResource>(
			frost::DeviceResource{ "frost | helloworld example", width, height });
		app.addPlugins<frost::DevicePlugin>();

		app.insertResource<frost::GraphicsResource>(
			frost::GraphicsResource{ width, height });
		app.addPlugins<frost::GraphicsPlugin>();
	}
	app.run();

	return 0;
}
