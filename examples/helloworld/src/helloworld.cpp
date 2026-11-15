/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

// Engine
#include <frost/ecs.h>
#include <frost/commandline.h>

// Example
#include "camera.h"
#include "scene.h"
#include <devmenu.h>

namespace
{
	/// Example plugin.
	///
	struct ExamplePlugin : frost::PluginI
	{
		virtual void build(frost::App& _app) override
		{
			_app.insertResource<SceneResource>(SceneResource{});

			_app.addSystems(frost::EcsStage::PostStartup, SceneSystem::startup);
			_app.addSystems(frost::EcsStage::Update, CameraSystem::update);
			_app.addSystems(frost::EcsStage::Update, SceneSystem::update);
			_app.addSystems(frost::EcsStage::PreShutdown, SceneSystem::shutdown);
		}
	};

} // namespace

static const frost::CommandLineOption s_options[] =
{
	{ 'h',   "help",   0, NULL,       "Display this help and exit." },
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
		app.addPlugins<ExamplePlugin>();

		app.insertResource<frost::DeviceResource>(
			frost::DeviceResource{ "Frostfall Engine | Helloworld Example", width, height });
		app.addPlugins<frost::DevicePlugin>();

		app.insertResource<frost::GraphicsResource>(
			frost::GraphicsResource{ width, height, frost::RendererType::Count /*auto*/ });
		app.addPlugins<frost::GraphicsPlugin>();

		app.insertResource<frost::DevmenuResource>();
		app.addPlugins<frost::DevmenuPlugin>();

		// Optional example entries, configured beside the plugin. The plugin owns the
		// built-in pages; this application owns its custom hierarchy and actions.
		struct ResetCameraItem { frost::Entity camera; };
		app.addSystems(frost::EcsStage::PostStartup,
			+[](frost::World& _world, frost::ResMutT<frost::DevmenuResource> _menu,
				frost::ResT<SceneResource> _scene)
			{
				_menu->custom = _world.spawn().id();
				const frost::Entity folder = _world.spawn(frost::DevmenuItemComponent{ "Example camera", 3 },
					frost::DevmenuFolderComponent{}).setParent(_menu->custom).id();
				const frost::Entity camera = _scene->instances[SceneResource::Player];
				_world.spawn(frost::DevmenuItemComponent{ "Speed", 0 },
					frost::DevmenuFloatComponent{ 0, 0.1, 100, 0.5, 1 },
					frost::devmenuComponent<&ExampleCameraComponent::speed>(camera)).setParent(folder);
				_world.spawn(frost::DevmenuItemComponent{ "Boost", 1 },
					frost::DevmenuFloatComponent{ 0, 1, 20, 0.5, 1 },
					frost::devmenuComponent<&ExampleCameraComponent::boost>(camera)).setParent(folder);
				_world.spawn(frost::DevmenuItemComponent{ "Sensitivity", 2 },
					frost::DevmenuFloatComponent{ 0, 0.0001, 0.02, 0.0001, 4 },
					frost::devmenuComponent<&ExampleCameraComponent::sensitivity>(camera)).setParent(folder);
				_world.spawn(frost::DevmenuItemComponent{ "Reset position", 3 },
					frost::DevmenuButtonComponent{}, ResetCameraItem{ camera }).setParent(folder);
			});
		app.addSystems(frost::EcsStage::Update,
			+[](frost::World& _world,
				frost::QueryT<const frost::DevmenuButtonComponent&, const ResetCameraItem&> _buttons)
			{
				_buttons.each([&](const frost::DevmenuButtonComponent& _button, const ResetCameraItem& _item)
				{
					if (_button.activated) { _world.getMut<frost::CameraComponent>(_item.camera)->position = { 0, 3, -5 }; }
				});
			});

	}
	app.run();

	return 0;
}
