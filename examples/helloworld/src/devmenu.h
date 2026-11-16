/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#ifndef FROST_EXAMPLE_HELLOWORLD_DEVMENU_H_HEADER_GUARD
#define FROST_EXAMPLE_HELLOWORLD_DEVMENU_H_HEADER_GUARD

// Engine
#include <frost/ecs.h>

// Engine Plugins
#include <devmenu.h>

namespace
{
	struct ResetCameraItem
	{
		frost::Entity camera;
	};

	struct DevmenuSystem
	{
		static void startup(frost::World& _world, frost::ResMutT<frost::DevmenuResource> _menu,
				frost::ResT<SceneResource> _scene)
		{
			_menu->custom = _world.spawn().id();

			const frost::Entity folder = _world.spawn(frost::DevmenuItemComponent{ "Camera", 3 },
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
		}

		static void update(frost::World& _world,
				frost::QueryT<const frost::DevmenuButtonComponent&, const ResetCameraItem&> _buttons)
		{
			_buttons.each([&](const frost::DevmenuButtonComponent& _button, const ResetCameraItem& _item)
			{
				if (_button.activated)
				{
					_world.getMut<frost::CameraComponent>(_item.camera)->position = { 0, 3, -5 };
				}
			});
		}
	};

} // namespace

#endif // FROST_EXAMPLE_HELLOWORLD_DEVMENU_H_HEADER_GUARD
