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
				frost::devmenuComponent<&CameraComponent::speed>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Boost", 1 },
				frost::DevmenuFloatComponent{ 0, 1, 20, 0.5, 1 },
				frost::devmenuComponent<&CameraComponent::boost>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Sensitivity", 2 },
				frost::DevmenuFloatComponent{ 0, 0.1, 10, 0.1, 1 },
				frost::devmenuComponent<&CameraComponent::sensitivity>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Mode (0 Orbit, 1 Flying)", 3 },
				frost::DevmenuIntComponent{ 0, CameraMode::Orbit, CameraMode::Flying, 1 },
				frost::devmenuComponent<&CameraComponent::mode>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Orbit distance", 4 },
				frost::DevmenuFloatComponent{ 0, 0.1, 100, 0.5, 1 },
				frost::devmenuComponent<&CameraComponent::distance>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Lag (seconds)", 5 },
				frost::DevmenuFloatComponent{ 0, 0, 2, 0.01, 2 },
				frost::devmenuComponent<&CameraComponent::lag>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Reset camera", 6 },
				frost::DevmenuButtonComponent{}, ResetCameraItem{ camera }).setParent(folder);
		}

		static void update(frost::World& _world,
				frost::QueryT<const frost::DevmenuButtonComponent&, const ResetCameraItem&> _buttons)
		{
			_buttons.each([&](const frost::DevmenuButtonComponent& _button, const ResetCameraItem& _item)
			{
				if (_button.activated)
				{
					frost::CameraComponent* camera = _world.getMut<frost::CameraComponent>(_item.camera);
					CameraComponent* controller = _world.getMut<CameraComponent>(_item.camera);
					if (NULL != camera && NULL != controller)
					{
						CameraSystem::reset(*camera, *controller,
							_world.get<frost::TransformComponent>(controller->target));
					}
				}
			});
		}
	};

} // namespace

#endif // FROST_EXAMPLE_HELLOWORLD_DEVMENU_H_HEADER_GUARD
