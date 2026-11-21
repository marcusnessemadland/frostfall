/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#ifndef FROST_EXAMPLE_HELLOWORLD_CAMERA_H_HEADER_GUARD
#define FROST_EXAMPLE_HELLOWORLD_CAMERA_H_HEADER_GUARD

// Engine
#include <frost/ecs.h>

// Engine Plugins
#include <device.h>
#include <graphics.h>
#include <devmenu.h>

namespace
{
	/// Camera mode enum.
	///
	struct CameraMode
	{
		enum Enum
		{
			Orbit,
			Flying
		};
	};

	/// Example camera component.
	///
	struct CameraControllerComponent
	{
		CameraMode::Enum mode = CameraMode::Orbit;
		CameraMode::Enum previousMode = CameraMode::Orbit;

		float distance = 5.0f;
		float lag = 0.15f;
		float speed = 10.0f;
		float boost = 3.0f;
		float sensitivity = 2.5f;

		frost::Vec3 desiredPosition = { 0.0f, 0.0f, 0.0f };
		frost::Quaternion desiredRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		frost::Vec3 startPosition = { 0, 0, 0 };
		frost::Quaternion startRotation = { 0, 0, 0, 1 };

		frost::Entity target; //!< Optional entity to orbit, must contain a transform component
	};

	/// Camera system.
	///
	struct CameraSystem
	{
		static void startup(frost::World& _world)
		{
			// Setup internal frost camera
			_world.query<frost::Entity, CameraControllerComponent&>().each(
			[&](frost::Entity _entity, CameraControllerComponent& _controller)
			{
				frost::CameraComponent camera;

				if (const auto* transform = _world.get<frost::TransformComponent>(_entity))
				{
					camera.position = transform->position;
					camera.rotation = transform->rotation;
				}
				_controller.startPosition = camera.position;
				_controller.startRotation = camera.rotation;

				// Reset values based on transform component of target
				reset(camera, _controller, _world.get<frost::TransformComponent>(_controller.target));

				_world.commands().entity(_entity).insert(camera);
			});
			_world.flush();
		}

		static void update(
			  frost::ResT<frost::TimeResource> _time
			, frost::ResT<frost::InputResource> _input
			, frost::ResT<frost::ActiveCameraResource> _activeCamera
			, frost::ResT<frost::DevmenuDesc> _menu
			, frost::QueryT<frost::CameraComponent&, CameraControllerComponent&> _cameras
			, frost::QueryT<const frost::TransformComponent&> _transforms
			)
		{
			const float delta = float(frost::min(_time->delta, 0.1));

			// Is input enabled
			const frost::GamepadState& gamepad = _input->gamepad;
			const bool inputEnabled = _input->focused && gamepad.connected && !_menu->visible;

			// Update the active example camera.
			_cameras.get(_activeCamera->entity,
			[&](frost::CameraComponent& _camera, CameraControllerComponent& _controller)
			{
				if (_controller.previousMode != _controller.mode)
				{
					_controller.desiredPosition = _camera.position;
					_controller.desiredRotation = _camera.rotation;
					_controller.previousMode = _controller.mode;
				}

				const frost::Vec3 forward = frost::mul({ 0.0f, 0.0f, 1.0f }, _controller.desiredRotation);

				// Update target position
				frost::Vec3 target = { 0.0f, 0.0f, 0.0f };

				_transforms.get(_controller.target,
				[&](const frost::TransformComponent& _transform)
				{
					// If target entity is valid and has a transform, use this as our target
					target = _transform.position;
				});

				// Calculate desired rotation
				if (inputEnabled && (0.0f != gamepad.rightX || 0.0f != gamepad.rightY))
				{
					const float yaw = frost::wrap(frost::atan2(forward.x, forward.z)
						+ gamepad.rightX * _controller.sensitivity * delta, frost::kPi2);

					const float pitch = frost::clamp(frost::asin(frost::clamp(-forward.y, -1.0f, 1.0f))
						+ gamepad.rightY * _controller.sensitivity * delta,
						-frost::toRad(89.0f), frost::toRad(89.0f));

					_controller.desiredRotation = frost::normalize(frost::mul(frost::rotateY(yaw), frost::rotateX(pitch)));
				}

				// Calculate desired position based on camera mode
				switch(_controller.mode)
				{
					case CameraMode::Orbit:
					{
						_controller.desiredPosition = frost::sub(target, frost::mul(forward, _controller.distance));
						break;
					}

					case CameraMode::Flying:
					{
						if (inputEnabled)
						{
							const frost::Vec3 right = frost::mul({ 1.0f, 0.0f, 0.0f }, _controller.desiredRotation);
							frost::Vec3 movement = frost::add(
								frost::add(frost::mul(forward, -gamepad.leftY), frost::mul(right, gamepad.leftX)),
								{ 0.0f, gamepad.rightTrigger - gamepad.leftTrigger, 0.0f });
							const float magnitude = frost::length(movement);
							if (1.0f < magnitude) { movement = frost::mul(movement, 1.0f / magnitude); }
							const bool boosted = gamepad.buttons[frost::GamepadButton::LeftStick];
							const float speed = _controller.speed * (boosted ? _controller.boost : 1.0f);
							_controller.desiredPosition = frost::add(_controller.desiredPosition, frost::mul(movement, speed * delta));
						}
						break;
					}
				}

				// Calculate camera lag
				float alpha = 1.0f;
				if (0.0f < _controller.lag)
				{
					alpha = 1.0f - frost::exp(-float(_time->delta) / _controller.lag);
				}

				// Update final position and rotation
				_camera.position = frost::lerp(_camera.position, _controller.desiredPosition, alpha);
				_camera.rotation = frost::normalize(frost::slerp(_camera.rotation, _controller.desiredRotation, alpha));
			});
		}

		static void reset(frost::CameraComponent& _camera, CameraControllerComponent& _controller, const frost::TransformComponent* _target)
		{
			if (CameraMode::Orbit == _controller.mode && NULL != _target)
			{
				_camera.position = frost::sub(_target->position, { 0.0f, 0.0f, _controller.distance });
			}
			else
			{
				_camera.position = _controller.startPosition;
			}

			if (CameraMode::Orbit == _controller.mode)
			{
				_camera.rotation = frost::Quaternion{ 0, 0, 0, 1 };
			}
			else
			{
				_camera.rotation = _controller.startRotation;
			}

			_controller.desiredPosition = _camera.position;
			_controller.desiredRotation = _camera.rotation;
			_controller.previousMode = _controller.mode;
		}
	};

} // namespace

#endif // FROST_EXAMPLE_HELLOWORLD_CAMERA_H_HEADER_GUARD
