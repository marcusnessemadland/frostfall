#ifndef FROST_EXAMPLE_HELLOWORLD_CAMERA_H_HEADER_GUARD
#define FROST_EXAMPLE_HELLOWORLD_CAMERA_H_HEADER_GUARD

// Engine
#include <frost/ecs.h>

// Engine Plugins
#include <device.h>
#include <graphics.h>

namespace
{
	struct ExampleCameraComponent
	{
		float speed = 10.0f;
		float boost = 3.0f;
		float sensitivity = 0.002f;
	};

	struct CameraSystem
	{
		static void update(
			  frost::ResT<frost::TimeResource> _time
			, frost::ResT<frost::InputResource> _input
			, frost::ResT<frost::ActiveCameraResource> _activeCamera
			, frost::QueryT<frost::CameraComponent&, const ExampleCameraComponent&> _cameras
			)
		{
			const float delta = float(frost::min(_time->delta, 0.1));

			// Dont process input if input is not in focus
			if (!_input->focused)
			{
				return;
			}

			// Update the active example camera.
			_cameras.get(_activeCamera->entity,
				[&](frost::CameraComponent& _camera, const ExampleCameraComponent& _controller)
				{
					// Rotate camera
					if (0 != (_input->mouseButtons & frost::MouseButton::Right)
					&& (0 != _input->mouseDeltaX || 0 != _input->mouseDeltaY) )
					{
						const frost::Vec3 forward = frost::mul({ 0.0f, 0.0f, 1.0f }, _camera.rotation);

						const float yaw = frost::wrap(frost::atan2(forward.x, forward.z)
							+ float(_input->mouseDeltaX) * _controller.sensitivity, frost::kPi2);
						const float pitch = frost::clamp(frost::asin(frost::clamp(-forward.y, -1.0f, 1.0f))
							+ float(_input->mouseDeltaY) * _controller.sensitivity,
							-frost::toRad(89.0f), frost::toRad(89.0f));

						_camera.rotation = frost::normalize(frost::mul(frost::rotateY(yaw), frost::rotateX(pitch)));
					}

					// Move camera
					const frost::Vec3 forward = frost::mul({ 0.0f, 0.0f, 1.0f }, _camera.rotation);
					const frost::Vec3 right = frost::mul({ 1.0f, 0.0f, 0.0f }, _camera.rotation);

					const float forwardInput = float(_input->keys[frost::Key::W]) - float(_input->keys[frost::Key::S]);
					const float rightInput = float(_input->keys[frost::Key::D]) - float(_input->keys[frost::Key::A]);
					const float upInput = float(_input->keys[frost::Key::E]) - float(_input->keys[frost::Key::Q]);

					const frost::Vec3 movement = frost::add(
						frost::add(frost::mul(forward, forwardInput), frost::mul(right, rightInput)),
						{ 0.0f, upInput, 0.0f });

					if (0.0f < frost::dot(movement, movement))
					{
						const bool boosted = _input->keys[frost::Key::LeftShift] || _input->keys[frost::Key::RightShift];
						const float speed = _controller.speed * (boosted ? _controller.boost : 1.0f);

						_camera.position = frost::add(_camera.position, frost::mul(frost::normalize(movement), speed * delta));
					}
				});
		}
	};

}

#endif // FROST_EXAMPLE_HELLOWORLD_CAMERA_H_HEADER_GUARD
