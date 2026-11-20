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
	/// Reset camera button data.
	///
	struct ResetCameraItem
	{
		frost::Entity camera;
	};

	/// Example extension of the devmenu.
	///
	struct DevmenuSystem
	{
		static void startup(frost::World& _world, frost::ResMutT<frost::DevmenuDesc> _menu,
				frost::ResT<frost::ActiveCameraResource> _activeCamera)
		{
			_menu->custom = _world.spawn().id();

			// Camera menu
			const frost::Entity folder = _world.spawn(frost::DevmenuItemComponent{ "Camera", 3 },
				frost::DevmenuFolderComponent{}).setParent(_menu->custom).id();

			const frost::Entity camera = _activeCamera->entity;

			_world.spawn(frost::DevmenuItemComponent{ "Speed", 0 },
				frost::DevmenuFloatComponent{ 0, 0.1, 100, 0.5, 1 },
				frost::devmenuComponent<&CameraControllerComponent::speed>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Boost", 1 },
				frost::DevmenuFloatComponent{ 0, 1, 20, 0.5, 1 },
				frost::devmenuComponent<&CameraControllerComponent::boost>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Sensitivity", 2 },
				frost::DevmenuFloatComponent{ 0, 0.1, 10, 0.1, 1 },
				frost::devmenuComponent<&CameraControllerComponent::sensitivity>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Mode (0 Orbit, 1 Flying)", 3 },
				frost::DevmenuIntComponent{ 0, CameraMode::Orbit, CameraMode::Flying, 1 },
				frost::devmenuComponent<&CameraControllerComponent::mode>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Orbit distance", 4 },
				frost::DevmenuFloatComponent{ 0, 0.1, 100, 0.5, 1 },
				frost::devmenuComponent<&CameraControllerComponent::distance>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Lag (seconds)", 5 },
				frost::DevmenuFloatComponent{ 0, 0, 2, 0.01, 2 },
				frost::devmenuComponent<&CameraControllerComponent::lag>(camera)).setParent(folder);
			_world.spawn(frost::DevmenuItemComponent{ "Reset camera", 6 },
				frost::DevmenuButtonComponent{}, ResetCameraItem{ camera }).setParent(folder);

			const frost::Entity graphics = _world.spawn(frost::DevmenuItemComponent{ "Graphics", 4 },
				frost::DevmenuFolderComponent{}).setParent(_menu->custom).id();
			_world.spawn(frost::DevmenuItemComponent{ "Instancing", 0 },
				frost::DevmenuBoolComponent{},
				frost::devmenuResource<&frost::GraphicsSettings::instancing>()).setParent(graphics);
			_world.spawn(frost::DevmenuItemComponent{ "Frustum culling", 1 },
				frost::DevmenuBoolComponent{},
				frost::devmenuResource<&frost::GraphicsSettings::frustumCulling>()).setParent(graphics);
			_world.spawn(frost::DevmenuItemComponent{ "LODs", 2 },
				frost::DevmenuBoolComponent{},
				frost::devmenuResource<&frost::GraphicsSettings::lods>()).setParent(graphics);
			_world.spawn(frost::DevmenuItemComponent{ "LOD pixels", 3 },
				frost::DevmenuFloatComponent{ 0, 1, 1024, 16, 0 },
				frost::devmenuResource<&frost::GraphicsSettings::lodPixels>()).setParent(graphics);
			_world.spawn(frost::DevmenuItemComponent{ "Geometry draws", 4 },
				frost::DevmenuIntComponent{ 0, 0, INT32_MAX, 1, true },
				frost::DevmenuBindingComponent{ {}, [](const frost::World& _world, frost::Entity)
				{
					return double(_world.getResource<frost::GraphicsStatsResource>()->geometry.draws);
				} }).setParent(graphics);
			_world.spawn(frost::DevmenuItemComponent{ "Shadow draws", 5 },
				frost::DevmenuIntComponent{ 0, 0, INT32_MAX, 1, true },
				frost::DevmenuBindingComponent{ {}, [](const frost::World& _world, frost::Entity)
				{
					return double(_world.getResource<frost::GraphicsStatsResource>()->shadows.draws);
				} }).setParent(graphics);
			_world.spawn(frost::DevmenuItemComponent{ "Shared geometry", 6 },
				frost::DevmenuIntComponent{ 0, 0, INT32_MAX, 1, true },
				frost::DevmenuBindingComponent{ {}, [](const frost::World& _world, frost::Entity)
				{
					return double(_world.getResource<frost::GraphicsStatsResource>()->sharedGeometries);
				} }).setParent(graphics);

			const frost::Entity shadows = _world.spawn(frost::DevmenuItemComponent{ "Shadows", 5 },
				frost::DevmenuFolderComponent{}).setParent(_menu->custom).id();
			_world.spawn(frost::DevmenuItemComponent{ "Distance scale", 0 },
				frost::DevmenuFloatComponent{ 0, 0, 2, 0.05, 2 },
				frost::devmenuResource<&frost::SunShadowResource::distanceScale>()).setParent(shadows);
			_world.spawn(frost::DevmenuItemComponent{ "Distance", 1 },
				frost::DevmenuFloatComponent{ 0, 0, 1000, 5, 0 },
				frost::devmenuResource<&frost::SunShadowResource::distance>()).setParent(shadows);
			_world.spawn(frost::DevmenuItemComponent{ "Caster distance", 2 },
				frost::DevmenuFloatComponent{ 0, 0, 1000, 5, 0 },
				frost::devmenuResource<&frost::SunShadowResource::casterDistance>()).setParent(shadows);
			_world.spawn(frost::DevmenuItemComponent{ "Split lambda", 3 },
				frost::DevmenuFloatComponent{ 0, 0, 1, 0.05, 2 },
				frost::devmenuResource<&frost::SunShadowResource::splitLambda>()).setParent(shadows);
			_world.spawn(frost::DevmenuItemComponent{ "Resolution", 4 },
				frost::DevmenuIntComponent{ 0, 256, 4096, 256 },
				frost::devmenuResource<&frost::SunShadowResource::resolution>()).setParent(shadows);
		}

		static void update(frost::World& _world,
				frost::QueryT<const frost::DevmenuButtonComponent&, const ResetCameraItem&> _buttons)
		{
			// On camera reset button press
			_buttons.each([&](const frost::DevmenuButtonComponent& _button, const ResetCameraItem& _item)
			{
				if (_button.activated)
				{
					frost::CameraComponent* camera = _world.getMut<frost::CameraComponent>(_item.camera);
					CameraControllerComponent* controller = _world.getMut<CameraControllerComponent>(_item.camera);

					// Reset camera
					if (NULL != camera && NULL != controller)
					{
						CameraSystem::reset(*camera, *controller, _world.get<frost::TransformComponent>(controller->target));
					}
				}
			});
		}
	};

} // namespace

#endif // FROST_EXAMPLE_HELLOWORLD_DEVMENU_H_HEADER_GUARD
