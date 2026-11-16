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
	struct CameraMode
	{
		enum Enum
		{
			Orbit,
			Flying
		};
	};

	struct CameraComponent
	{
		CameraMode::Enum mode = CameraMode::Orbit;
		frost::Entity target;
		float distance = 5.0f;
		float lag = 0.15f;
		float speed = 10.0f;
		float boost = 3.0f;
		float sensitivity = 2.5f; // Radians per second at full stick deflection.

		frost::Vec3 desiredPosition = { 0.0f, 0.0f, 0.0f };
		frost::Quaternion desiredRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		CameraMode::Enum previousMode = CameraMode::Orbit;
		bool initialized = false;
	};

	struct CameraSystem
	{
		static void reset(frost::CameraComponent& _camera, CameraComponent& _controller,
			const frost::TransformComponent* _target)
		{
			_camera.position = CameraMode::Orbit == _controller.mode && NULL != _target
				? frost::sub(_target->position, { 0.0f, 0.0f, _controller.distance })
				: frost::Vec3{ 0.0f, 3.0f, -5.0f };
			_camera.rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
			_controller.desiredPosition = _camera.position;
			_controller.desiredRotation = _camera.rotation;
			_controller.previousMode = _controller.mode;
			_controller.initialized = true;
		}

		static void update(
			  frost::ResT<frost::TimeResource> _time
			, frost::ResT<frost::InputResource> _input
			, frost::ResT<frost::ActiveCameraResource> _activeCamera
			, frost::ResT<frost::DevmenuResource> _menu
			, frost::QueryT<frost::CameraComponent&, CameraComponent&> _cameras
			, frost::QueryT<const frost::TransformComponent&> _transforms
			)
		{
			const float delta = float(frost::min(_time->delta, 0.1));

			const frost::GamepadState& gamepad = _input->gamepad;
			const bool inputEnabled = _input->focused && gamepad.connected && !_menu->visible;

			// Update the active example camera.
			_cameras.get(_activeCamera->entity,
				[&](frost::CameraComponent& _camera, CameraComponent& _controller)
			{
				if (!_controller.initialized || _controller.previousMode != _controller.mode)
				{
					_controller.desiredPosition = _camera.position;
					_controller.desiredRotation = _camera.rotation;
					_controller.previousMode = _controller.mode;
					_controller.initialized = true;
				}

				frost::Vec3 target = { 0.0f, 0.0f, 0.0f };
				if (CameraMode::Orbit == _controller.mode
				&& !_transforms.get(_controller.target,
					[&](const frost::TransformComponent& _transform) { target = _transform.position; }))
				{
					return;
				}

				// Rotate the desired pose so lag never discards stick input.
				if (inputEnabled && (0.0f != gamepad.rightX || 0.0f != gamepad.rightY))
				{
					const frost::Vec3 forward = frost::mul({ 0.0f, 0.0f, 1.0f }, _controller.desiredRotation);
					const float yaw = frost::wrap(frost::atan2(forward.x, forward.z)
						+ gamepad.rightX * _controller.sensitivity * delta, frost::kPi2);
					const float pitch = frost::clamp(frost::asin(frost::clamp(-forward.y, -1.0f, 1.0f))
						+ gamepad.rightY * _controller.sensitivity * delta,
						-frost::toRad(89.0f), frost::toRad(89.0f));
					_controller.desiredRotation = frost::normalize(frost::mul(frost::rotateY(yaw), frost::rotateX(pitch)));
				}

				const frost::Vec3 forward = frost::mul({ 0.0f, 0.0f, 1.0f }, _controller.desiredRotation);
				if (CameraMode::Orbit == _controller.mode)
				{
					_controller.desiredPosition = frost::sub(target, frost::mul(forward, _controller.distance));
				}
				else if (inputEnabled)
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

				const float alpha = 0.0f < _controller.lag
					? 1.0f - frost::exp(-float(_time->delta) / _controller.lag) : 1.0f;
				_camera.position = frost::lerp(_camera.position, _controller.desiredPosition, alpha);
				_camera.rotation = frost::normalize(frost::slerp(_camera.rotation, _controller.desiredRotation, alpha));
			});
		}
	};

} // namespace

#endif // FROST_EXAMPLE_HELLOWORLD_CAMERA_H_HEADER_GUARD
