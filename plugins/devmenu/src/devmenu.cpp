/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <devmenu.h>
#include <graphics.h>
#include <frost/bufferstorage.h>
#include <frost/math.h>

namespace frost
{
	struct DevmenuEntry { Entity entity; int32_t order; };
	struct DevmenuPageComponent { int32_t selection = 0; uint32_t first = 0; };
	struct DevmenuTargetComponent
	{
		enum Kind { Sky, Light, Camera };
		Kind kind;
	};

	struct DevmenuState
	{
		DevmenuState() = default;
		DevmenuState(DevmenuState&& _other)
			: canvas(_other.canvas), panel(_other.panel), title(_other.title), separator(_other.separator), list(_other.list)
			, current(_other.current), editing(_other.editing), editingTarget(_other.editingTarget)
			, frameTime(_other.frameTime), fps(_other.fps), frames(_other.frames), entities(_other.entities)
			, dimensions(_other.dimensions), focus(_other.focus)
			, debugView(_other.debugView)
			, count(_other.count), rowCount(_other.rowCount), original(_other.original)
			, repeatAt(_other.repeatAt), repeatDirection(_other.repeatDirection)
		{
			memCopy(graphicsStats, _other.graphicsStats, sizeof(graphicsStats));
			memCopy(moonStats, _other.moonStats, sizeof(moonStats));
			entries = frost::move(_other.entries);
			rows = frost::move(_other.rows);
		}

		Entity canvas, panel, title, separator, list;
		Entity current, editing, editingTarget;
		Entity frameTime, fps, frames, entities, dimensions, focus;
		Entity debugView, graphicsStats[17], moonStats[4];
		DynamicStorageT<DevmenuEntry> entries;
		DynamicStorageT<Entity> rows;
		uint32_t count = 0, rowCount = 0;
		double original = 0.0;
		double repeatAt = 0.0;
		int32_t repeatDirection = 0;
	};

	static Entity devmenuFolder(World& _world, Entity _parent, const char* _label, int32_t _order = 0)
	{
		return _world.spawn(DevmenuItemComponent{ _label, _order }, DevmenuFolderComponent{},
			DevmenuPageComponent{}).setParent(_parent).id();
	}

	template<typename ValueT>
	static Entity devmenuValue(World& _world, Entity _parent, const char* _label, ValueT _value,
		int32_t _order, DevmenuBindingComponent _binding)
	{
		return _world.spawn(DevmenuItemComponent{ _label, _order }, _value, _binding).setParent(_parent).id();
	}

	static Entity devmenuReadout(World& _world, Entity _parent, const char* _label, int32_t _order)
	{
		return _world.spawn(DevmenuItemComponent{ _label, _order }, DevmenuReadoutComponent{}).setParent(_parent).id();
	}

	static void devmenuSetReadout(World& _world, Entity _entity, const char* _value)
	{
		_world.getMut<DevmenuReadoutComponent>(_entity)->text.set(_value);
	}

	static void devmenuWrite(World& _world, Entity _entity, double _value)
	{
		if (auto* value = _world.getMut<DevmenuFloatComponent>(_entity)) { value->value = _value; }
		if (auto* value = _world.getMut<DevmenuIntComponent>(_entity)) { value->value = int32_t(_value); }
		if (auto* value = _world.getMut<DevmenuBoolComponent>(_entity)) { value->value = 0.0 != _value; }
		if (const auto* binding = _world.get<DevmenuBindingComponent>(_entity))
		{
			if (NULL != binding->write && (NULL == binding->available || binding->available(_world, binding->target)))
			{
				binding->write(_world, binding->target, _value);
			}
		}
	}

	static void devmenuCancel(World& _world, DevmenuState& _state)
	{
		const auto* binding = _world.get<DevmenuBindingComponent>(_state.editing);
		if (_world.contains(_state.editing) && (NULL == binding || binding->target == _state.editingTarget))
		{
			devmenuWrite(_world, _state.editing, _state.original);
		}
		_state.editing = {};
	}

	static bool devmenuVisible(World& _world, Entity _entity)
	{
		const auto* item = _world.get<DevmenuItemComponent>(_entity);
		if (NULL == item || !item->visible) { return false; }
		const auto* binding = _world.get<DevmenuBindingComponent>(_entity);
		return NULL == binding || NULL == binding->available || binding->available(_world, binding->target);
	}

	static void devmenuAppend(World& _world, Entity _parent, DevmenuState& _state)
	{
		for (Entity child = _world.getFirstChild(_parent); _world.contains(child); child = _world.getNextSibling(child))
		{
			if (!devmenuVisible(_world, child)) { continue; }
			const int32_t order = _world.get<DevmenuItemComponent>(child)->order;
			if (_state.count == _state.entries.getCapacity())
			{
				_state.entries.setCapacity(max(16u, _state.count * 2), _state.count);
			}
			uint32_t index = _state.count++;
			while (index > 0)
			{
				const DevmenuEntry prev = *_state.entries.getPtr(index - 1);
				if (prev.order < order || (prev.order == order && prev.entity.idx < child.idx)) { break; }
				*_state.entries.getPtr(index--) = prev;
			}
			*_state.entries.getPtr(index) = { child, order };
		}
	}

	static DevmenuPageComponent& devmenuPage(World& _world, Entity _entity)
	{
		if (auto* page = _world.getMut<DevmenuPageComponent>(_entity)) { return *page; }
		return *_world.emplace<DevmenuPageComponent>(_entity);
	}

	struct DevmenuSystem
	{
		static void startup(World& _world, ResMutT<DevmenuDesc> _menu, ResMutT<DevmenuState> _state)
		{
			_state->entries.setAllocator(_world.getAllocator());
			_state->rows.setAllocator(_world.getAllocator());
			_menu->root = _world.spawn(DevmenuFolderComponent{}, DevmenuPageComponent{}).id();
			_menu->engine = devmenuFolder(_world, _menu->root, "Engine", 0);
			_menu->device = devmenuFolder(_world, _menu->root, "Device", 1);
			_menu->graphics = devmenuFolder(_world, _menu->root, "Graphics", 2);
			_state->current = _menu->root;

			_state->fps = devmenuReadout(_world, _menu->engine, "FPS", 0);
			_state->frameTime = devmenuReadout(_world, _menu->engine, "Frame (ms)", 1);
			_state->frames = devmenuReadout(_world, _menu->engine, "Frame count", 2);
			_state->entities = devmenuReadout(_world, _menu->engine, "Entities", 3);
			devmenuValue(_world, _menu->engine, "Fixed step (s)",
				DevmenuFloatComponent{ 0, 1.0 / 1000, 1.0, 0.001, 4 }, 4, devmenuResource<&TimeResource::fixedDelta>());
			_state->dimensions = devmenuReadout(_world, _menu->device, "Drawable", 0);
			_state->focus = devmenuReadout(_world, _menu->device, "Input focus", 1);

			const Entity sky = devmenuFolder(_world, _menu->graphics, "Sky", 0);
			_world.insert(sky, DevmenuTargetComponent{ DevmenuTargetComponent::Sky });
			devmenuValue(_world, sky, "Time", DevmenuFloatComponent{ 0, 0, 24, 0.25, 2 }, 0,
				devmenuComponent<&SkyComponent::time>({}));
			devmenuValue(_world, sky, "Hours per second", DevmenuFloatComponent{ 0, -24, 24, 0.05, 2 }, 1,
				devmenuComponent<&SkyComponent::hps>({}));
			devmenuValue(_world, sky, "Month", DevmenuIntComponent{ 0, 0, 11, 1 }, 2,
				devmenuComponent<&SkyComponent::month>({}));
			devmenuValue(_world, sky, "Latitude", DevmenuFloatComponent{ 0, -90, 90, 1, 1 }, 3,
				devmenuComponent<&SkyComponent::latitude>({}));
			devmenuValue(_world, sky, "Turbidity", DevmenuFloatComponent{ 0, 1.9, 10, 0.1, 2 }, 4,
				devmenuComponent<&SkyComponent::turbidity>({}));

			const Entity sun = devmenuFolder(_world, sky, "Sun", 5);
			_world.insert(sun, DevmenuTargetComponent{ DevmenuTargetComponent::Sky });
			devmenuValue(_world, sun, "Intensity", DevmenuFloatComponent{ 0, 0, 100, 0.1, 2 }, 0,
				devmenuComponent<&SkyComponent::sunIntensity>({}));

			const Entity moon = devmenuFolder(_world, sky, "Moon", 6);
			_world.insert(moon, DevmenuTargetComponent{ DevmenuTargetComponent::Sky });
			devmenuValue(_world, moon, "Cycle (0 new, 0.5 full)", DevmenuFloatComponent{ 0, 0, 1, 0.025, 3 }, 0,
				devmenuComponent<&SkyComponent::moonCycle>({}));
			devmenuValue(_world, moon, "Pause cycle", DevmenuBoolComponent{}, 1,
				devmenuComponent<&SkyComponent::pauseMoonCycle>({}));
			devmenuValue(_world, moon, "Angular diameter (deg)", DevmenuFloatComponent{ 0, 0.01, 10, 0.05, 2 }, 2,
				devmenuComponent<&SkyComponent::moonSize>({}));
			devmenuValue(_world, moon, "Intensity", DevmenuFloatComponent{ 0, 0, 100, 0.1, 2 }, 3,
				devmenuComponent<&SkyComponent::moonIntensity>({}));
			const char* tintLabels[] = { "Tint R", "Tint G", "Tint B" };
			for (uint8_t ii = 0; ii < 3; ++ii)
			{
				DevmenuBindingComponent binding = devmenuComponent<&SkyComponent::moonIntensity>({});
				if (ii == 0)
				{
					binding.read = [](const World& _w, Entity _e) { return double(_w.get<SkyComponent>(_e)->moonTint.x); };
					binding.write = [](World& _w, Entity _e, double _v) { _w.getMut<SkyComponent>(_e)->moonTint.x = float(_v); };
				}
				else if (ii == 1)
				{
					binding.read = [](const World& _w, Entity _e) { return double(_w.get<SkyComponent>(_e)->moonTint.y); };
					binding.write = [](World& _w, Entity _e, double _v) { _w.getMut<SkyComponent>(_e)->moonTint.y = float(_v); };
				}
				else
				{
					binding.read = [](const World& _w, Entity _e) { return double(_w.get<SkyComponent>(_e)->moonTint.z); };
					binding.write = [](World& _w, Entity _e, double _v) { _w.getMut<SkyComponent>(_e)->moonTint.z = float(_v); };
				}
				devmenuValue(_world, moon, tintLabels[ii], DevmenuFloatComponent{ 0, 0, 4, 0.05, 2 }, 4 + ii, binding);
			}

			const char* moonLabels[] = { "Illuminated surface (%)", "Elevation (deg)", "Direct illumination", "Shadow source" };
			for (uint32_t ii = 0; ii < FROST_COUNTOF(moonLabels); ++ii)
			{
				_state->moonStats[ii] = devmenuReadout(_world, moon, moonLabels[ii], int32_t(7 + ii));
			}

			const Entity light = devmenuFolder(_world, _menu->graphics, "Directional light", 1);
			_world.insert(light, DevmenuTargetComponent{ DevmenuTargetComponent::Light });
			devmenuValue(_world, light, "Intensity", DevmenuFloatComponent{ 0, 0, 100, 0.1, 2 }, 0,
				devmenuComponent<&DirectionalLightComponent::intensity>({}));

			const Entity camera = devmenuFolder(_world, _menu->graphics, "Active camera", 2);
			_world.insert(camera, DevmenuTargetComponent{ DevmenuTargetComponent::Camera });
			devmenuValue(_world, camera, "FOV", DevmenuFloatComponent{ 0, 1, 179, 1, 1 }, 0,
				devmenuComponent<&CameraComponent::fov>({}));
			devmenuValue(_world, camera, "Ortho height", DevmenuFloatComponent{ 0, 0.1, 1000, 0.5, 1 }, 1,
				devmenuComponent<&CameraComponent::orthographicHeight>({}));
			devmenuValue(_world, camera, "Exposure (stops)", DevmenuFloatComponent{ 0, -20, 20, 0.25, 2 }, 2,
				devmenuComponent<&CameraComponent::exposure>({}));


			const Entity output = devmenuFolder(_world, _menu->graphics, "Output", 3);
			devmenuValue(_world, output, "Prevent banding", DevmenuBoolComponent{}, 0,
				devmenuResource<&GraphicsDesc::preventBanding>());
			devmenuValue(_world, output, "Debug view", DevmenuIntComponent{ 0, 0, GraphicsDebugView::Count - 1, 1 }, 1,
				devmenuResource<&GraphicsDesc::debugView>());
			_state->debugView = devmenuReadout(_world, output, "View", 2);
			devmenuValue(_world, output, "Depth range", DevmenuFloatComponent{ 0, 0.1, 10000, 10, 1 }, 3,
				devmenuResource<&GraphicsDesc::debugDepth>());
			devmenuValue(_world, output, "GPU profiling", DevmenuBoolComponent{}, 4,
				devmenuResource<&GraphicsDesc::gpuProfiling>());
			const Entity statistics = devmenuFolder(_world, _menu->graphics, "Statistics", 4);
			const char* labels[] = { "GPU (ms)", "Geometry (ms)", "Lighting (ms)", "Sky (ms)",
				"Tone map (ms)", "Debug (ms)", "UI (ms)", "Draw calls", "Textures", "Targets (MiB)",
				"Shadow atlas (ms)",
				"Cascade 0 casters", "Cascade 1 casters", "Cascade 2 casters", "Cascade 3 casters",
				"Shadow draw calls", "Shadow triangles" };
			for (uint32_t ii = 0; ii < FROST_COUNTOF(labels); ++ii)
			{
				_state->graphicsStats[ii] = devmenuReadout(_world, statistics, labels[ii], int32_t(ii));
			}

			const Entity shadows = devmenuFolder(_world, _menu->graphics, "Directional shadows", 5);
			devmenuValue(_world, shadows, "Tile resolution", DevmenuIntComponent{ 0, 256, 4096, 256 }, 0,
				devmenuResource<&SunShadowResource::resolution>());
			devmenuValue(_world, shadows, "Distance", DevmenuFloatComponent{ 0, 0, 1000, 5, 1 }, 1,
				devmenuResource<&SunShadowResource::distance>());
			devmenuValue(_world, shadows, "Split lambda", DevmenuFloatComponent{ 0, 0, 1, 0.05, 2 }, 2,
				devmenuResource<&SunShadowResource::splitLambda>());
			devmenuValue(_world, shadows, "Caster distance", DevmenuFloatComponent{ 0, 0, 1000, 5, 1 }, 3,
				devmenuResource<&SunShadowResource::casterDistance>());
			devmenuValue(_world, shadows, "Depth bias", DevmenuFloatComponent{ 0, 0, 0.01, 0.0001, 4 }, 4,
				devmenuResource<&SunShadowResource::depthBias>());
			devmenuValue(_world, shadows, "Normal bias (texels)", DevmenuFloatComponent{ 0, 0, 5, 0.1, 2 }, 5,
				devmenuResource<&SunShadowResource::normalBias>());
			devmenuValue(_world, shadows, "Cascade blend", DevmenuFloatComponent{ 0, 0, 0.3, 0.01, 2 }, 6,
				devmenuResource<&SunShadowResource::blend>());
			devmenuValue(_world, shadows, "Filter radius", DevmenuFloatComponent{ 0, 0, 3, 0.25, 2 }, 7,
				devmenuResource<&SunShadowResource::filterRadius>());

			devmenuValue(_world, shadows, "LODs", DevmenuBoolComponent{}, 8,
				devmenuResource<&SunShadowResource::lods>());
			devmenuValue(_world, shadows, "Instancing", DevmenuBoolComponent{}, 9,
				devmenuResource<&SunShadowResource::instancing>());

			_state->canvas = _world.spawn(UiCanvasComponent{ _menu->scale, 1000, _menu->visible }).id();
			_state->panel = _world.spawn(UiNodeComponent{}, UiLayoutComponent{ UiLayoutComponent::Column, 6, 2 },
				UiRectComponent{ 0x383838ef, 0xb0b0b0ff, 1 }).setParent(_state->canvas).id();
			_state->title = _world.spawn(UiNodeComponent{}, UiTextComponent{ "Dev Menu", 0x00ffffff })
				.setParent(_state->panel).id();
			_state->separator = _world.spawn(UiNodeComponent{ .order = 1 }, UiRectComponent{ 0x808080ff })
				.setParent(_state->panel).id();
			_state->list = _world.spawn(UiNodeComponent{ .order = 2 }, UiLayoutComponent{ UiLayoutComponent::Column, 0, 2 })
				.setParent(_state->panel).id();
		}

		static void refresh(World& _world, const TimeResource& _time, const InputResource& _input,
			const DisplayResource& _display, DevmenuState& _state)
		{
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "%.1f", _time.delta > 0 ? 1.0 / _time.delta : 0);
			devmenuSetReadout(_world, _state.fps, buffer);
			snprintf(buffer, sizeof(buffer), "%.2f", _time.delta * 1000);
			devmenuSetReadout(_world, _state.frameTime, buffer);
			snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)_time.frame);
			devmenuSetReadout(_world, _state.frames, buffer);
			snprintf(buffer, sizeof(buffer), "%u", _world.getEntityCount());
			devmenuSetReadout(_world, _state.entities, buffer);
			snprintf(buffer, sizeof(buffer), "%u x %u", _display.width, _display.height);
			devmenuSetReadout(_world, _state.dimensions, buffer);
			devmenuSetReadout(_world, _state.focus, _input.focused ? "Yes" : "No");
			if (const GraphicsDesc* graphics = _world.getResource<GraphicsDesc>())
			{
				const char* views[] = { "Scene", "Base color", "World normals", "Device depth",
					"Linear depth", "World position", "HDR luminance",
					"Metallic", "Roughness", "Occlusion", "Emissive",
					"Shadow cascades", "Shadow visibility",
					"Shadow map 0", "Shadow map 1", "Shadow map 2", "Shadow map 3", "Shadow atlas",
					"Lunar sky", "Moon direct light", "Moon disk" };
				devmenuSetReadout(_world, _state.debugView, views[clamp(int32_t(graphics->debugView), 0, int32_t(FROST_COUNTOF(views) - 1))]);
			}
			if (const GraphicsStatsResource* stats = _world.getResource<GraphicsStatsResource>())
			{
				const double times[] = { stats->gpuMs, stats->geometryMs, stats->lightMs, stats->skyMs,
					stats->toneMapMs, stats->debugViewMs, stats->uiMs };
				for (uint32_t ii = 0; ii < FROST_COUNTOF(times); ++ii)
				{
					if (times[ii] >= 0) { snprintf(buffer, sizeof(buffer), "%.3f", times[ii]); }
					else { snprintf(buffer, sizeof(buffer), "Unavailable"); }
					devmenuSetReadout(_world, _state.graphicsStats[ii], buffer);
				}
				snprintf(buffer, sizeof(buffer), "%u", stats->draws);
				devmenuSetReadout(_world, _state.graphicsStats[7], buffer);
				snprintf(buffer, sizeof(buffer), "%u", stats->textures);
				devmenuSetReadout(_world, _state.graphicsStats[8], buffer);
				snprintf(buffer, sizeof(buffer), "%.2f", double(stats->renderTargetBytes) / (1024 * 1024));
				devmenuSetReadout(_world, _state.graphicsStats[9], buffer);
				if (stats->shadowMs >= 0) { snprintf(buffer, sizeof(buffer), "%.3f", stats->shadowMs); }
				else { snprintf(buffer, sizeof(buffer), "Unavailable"); }
				devmenuSetReadout(_world, _state.graphicsStats[10], buffer);
				for (uint32_t ii = 0; ii < 4; ++ii)
				{
					snprintf(buffer, sizeof(buffer), "%u", stats->shadowCasters[ii]);
					devmenuSetReadout(_world, _state.graphicsStats[11 + ii], buffer);
				}
				snprintf(buffer, sizeof(buffer), "%u", stats->shadowDraws);
				devmenuSetReadout(_world, _state.graphicsStats[15], buffer);
				snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)stats->shadowTriangles);
				devmenuSetReadout(_world, _state.graphicsStats[16], buffer);
			}

			// The first sky/light by entity ID and the explicitly active camera.
			Entity targets[3];
			_world.query<Entity, const SkyComponent&>().each([&](Entity _entity, const SkyComponent&)
			{
				if (!_world.contains(targets[0]) || _entity.idx < targets[0].idx) { targets[0] = _entity; }
			});
			_world.query<Entity, const DirectionalLightComponent&>().each([&](Entity _entity, const DirectionalLightComponent&)
			{
				if (!_world.contains(targets[1]) || _entity.idx < targets[1].idx) { targets[1] = _entity; }
			});
			if (const SkyComponent* sky = _world.get<SkyComponent>(targets[0]))
			{
				snprintf(buffer, sizeof(buffer), "%.1f", sky->moonPhase * 100.0f);
				devmenuSetReadout(_world, _state.moonStats[0], buffer);
				snprintf(buffer, sizeof(buffer), "%.2f", sky->moonElevation);
				devmenuSetReadout(_world, _state.moonStats[1], buffer);
				snprintf(buffer, sizeof(buffer), "%.5f", sky->moonIlluminance);
				devmenuSetReadout(_world, _state.moonStats[2], buffer);
				const DirectionalLightComponent* light = _world.get<DirectionalLightComponent>(
					_world.getResource<SunShadowResource>()->light);
				devmenuSetReadout(_world, _state.moonStats[3], NULL == light ? "Disabled"
					: (dot(normalize(light->direction), sky->moonLightDirection) > 0.99999f ? "Moon" : "Sun / other"));
			}

			targets[2] = _world.getResource<ActiveCameraResource>()->entity;
			_world.query<Entity, const DevmenuTargetComponent&, DevmenuItemComponent&>().each(
				[&](Entity _folder, const DevmenuTargetComponent& _target, DevmenuItemComponent& _item)
			{
				const Entity target = targets[_target.kind];
				_item.visible = _world.contains(target);
				for (Entity child = _world.getFirstChild(_folder); _world.contains(child); child = _world.getNextSibling(child))
				{
					if (auto* binding = _world.getMut<DevmenuBindingComponent>(child))
					{
						if (_state.editing == child && binding->target != target) { devmenuCancel(_world, _state); }
						binding->target = target;
					}
				}
			});
			_world.query<Entity, const DevmenuBindingComponent&>().each([&](Entity _entity, const DevmenuBindingComponent& _binding)
			{
				if (NULL != _binding.available && !_binding.available(_world, _binding.target)) { return; }
				const double value = _binding.read(_world, _binding.target);
				if (auto* item = _world.getMut<DevmenuFloatComponent>(_entity)) { item->value = value; }
				if (auto* item = _world.getMut<DevmenuIntComponent>(_entity)) { item->value = int32_t(value); }
				if (auto* item = _world.getMut<DevmenuBoolComponent>(_entity)) { item->value = 0.0 != value; }
			});
		}

		static void goBack(World& _world, const DevmenuDesc& _menu, DevmenuState& _state)
		{
			const Entity parent = _world.getParent(_state.current);
			_state.current = parent == _menu.custom || !_world.contains(parent) ? _menu.root : parent;
		}

		static int32_t direction(const InputResource& _input, const TimeResource& _time,
			const DevmenuDesc& _menu, DevmenuState& _state)
		{
			const int32_t held = int32_t(_input.gamepad.buttons[_menu.up]) - int32_t(_input.gamepad.buttons[_menu.down]);
			const int32_t pressed = int32_t(_input.gamepad.pressed[_menu.up]) - int32_t(_input.gamepad.pressed[_menu.down]);
			if (0 != pressed)
			{
				_state.repeatDirection = held;
				_state.repeatAt = _time.elapsed + _menu.repeatDelay;
				return pressed; // A complete tap may arrive between two event pumps.
			}
			if (0 == held) { _state.repeatDirection = 0; return 0; }
			if (held != _state.repeatDirection)
			{
				_state.repeatDirection = held;
				_state.repeatAt = _time.elapsed + _menu.repeatDelay;
				return held;
			}
			if (_time.elapsed >= _state.repeatAt)
			{
				_state.repeatAt = _time.elapsed + _menu.repeatInterval;
				return held;
			}
			return 0;
		}

		static void input(World& _world, DevmenuDesc& _menu, DevmenuState& _state,
			const InputResource& _input, const TimeResource& _time)
		{
			_world.query<DevmenuButtonComponent&>().each([&](DevmenuButtonComponent& _button)
			{
				_button.activated = false;
				if (!_input.focused || !_input.gamepad.connected
				|| !_input.gamepad.buttons[_menu.select] || !_menu.visible) { _button.pressed = false; }
			});
			if (_input.focused && _input.gamepad.connected && _input.gamepad.pressed[_menu.toggle])
			{
				devmenuCancel(_world, _state);
				_menu.visible = !_menu.visible;
				_state.repeatDirection = 0;
			}
			if (!_menu.visible || !_input.focused || !_input.gamepad.connected)
			{
				devmenuCancel(_world, _state);
				_state.repeatDirection = 0;
				return;
			}
			bool attached = false;
			for (Entity ancestor = _state.current; _world.contains(ancestor); ancestor = _world.getParent(ancestor))
			{
				if (ancestor == _menu.root || ancestor == _menu.custom) { attached = true; break; }
				const auto* item = _world.get<DevmenuItemComponent>(ancestor);
				if (NULL != item && !item->visible) { break; }
			}
			if (!attached)
			{
				devmenuCancel(_world, _state);
				_state.current = _menu.root;
			}
			if (_input.gamepad.pressed[_menu.back])
			{
				if (_world.contains(_state.editing)) { devmenuCancel(_world, _state); }
				else if (_state.current != _menu.root) { goBack(_world, _menu, _state); }
				else { _menu.visible = false; }
				return;
			}
			if (const auto* binding = _world.get<DevmenuBindingComponent>(_state.editing))
			{
				if (binding->target != _state.editingTarget) { devmenuCancel(_world, _state); }
			}
			const int32_t movement = direction(_input, _time, _menu, _state);
			if (_world.contains(_state.editing))
			{
				if (!devmenuVisible(_world, _state.editing)) { devmenuCancel(_world, _state); return; }
				if (_input.gamepad.pressed[_menu.select]) { _state.editing = {}; return; }
				if (auto* value = _world.get<DevmenuFloatComponent>(_state.editing))
				{
					if (0 != movement) { devmenuWrite(_world, _state.editing,
						clamp(value->value + movement * value->step, value->minimum, value->maximum)); }
				}
				if (auto* value = _world.get<DevmenuIntComponent>(_state.editing))
				{
					if (0 != movement) { devmenuWrite(_world, _state.editing,
						double(clamp(int64_t(value->value) + int64_t(movement) * value->step,
							int64_t(value->minimum), int64_t(value->maximum)))); }
				}
				return;
			}
			_state.count = 0;
			devmenuAppend(_world, _state.current, _state);
			if (_state.current == _menu.root) { devmenuAppend(_world, _menu.custom, _state); }
			DevmenuPageComponent& page = devmenuPage(_world, _state.current);
			const int32_t first = _state.current == _menu.root ? 0 : -1;
			const int32_t last = int32_t(_state.count) - 1;
			page.selection = clamp(page.selection, first, max(first, last));
			if (0 != movement)
			{
				page.selection -= movement;
				if (page.selection < first) { page.selection = last; }
				if (page.selection > last) { page.selection = first; }
			}
			if (!_input.gamepad.pressed[_menu.select]) { return; }
			if (-1 == page.selection) { goBack(_world, _menu, _state); return; }
			if (0 == _state.count) { return; }
			const Entity selected = _state.entries.getPtr(uint32_t(page.selection))->entity;
			if (_world.has<DevmenuFolderComponent>(selected)) { _state.current = selected; return; }
			if (auto* button = _world.getMut<DevmenuButtonComponent>(selected))
			{
				button->activated = true; button->pressed = true;
			}
			const auto* binding = _world.get<DevmenuBindingComponent>(selected);
			if (NULL != binding && NULL == binding->write) { return; }
			_state.editingTarget = NULL != binding ? binding->target : Entity{};
			if (auto* value = _world.get<DevmenuBoolComponent>(selected))
			{
				if (!value->readOnly) { devmenuWrite(_world, selected, value->value ? 0 : 1); }
			}
			if (auto* value = _world.get<DevmenuFloatComponent>(selected))
			{
				FROST_ASSERT(value->minimum <= value->maximum && value->step > 0, "Invalid Devmenu float range/step.");
				if (!value->readOnly) { _state.editing = selected; _state.original = value->value; }
			}
			if (auto* value = _world.get<DevmenuIntComponent>(selected))
			{
				FROST_ASSERT(value->minimum <= value->maximum && value->step > 0, "Invalid Devmenu integer range/step.");
				if (!value->readOnly) { _state.editing = selected; _state.original = value->value; }
			}
		}

		static void row(World& _world, Entity _row, Entity _item, bool _back, bool _selected,
			const DevmenuDesc& _menu, const DevmenuState& _state, const TimeResource& _time)
		{
			UiTextComponent& text = *_world.getMut<UiTextComponent>(_row);
			text.height = _menu.textHeight;
			text.color = 0xe8e8e8ff;
			char buffer[1024], value[256] = {};
			const char* label = _back ? "..." : _world.get<DevmenuItemComponent>(_item)->label.getCPtr();
			const char* suffix = "";
			if (!_back)
			{
				if (_world.has<DevmenuFolderComponent>(_item)) { suffix = "..."; }
				if (const auto* button = _world.get<DevmenuButtonComponent>(_item))
				{
					if (button->pressed) { label = ""; }
				}
				if (const auto* boolean = _world.get<DevmenuBoolComponent>(_item))
				{
					if (boolean->value) { text.color = 0xffff00ff; }
				}
				if (const auto* integer = _world.get<DevmenuIntComponent>(_item))
				{
					snprintf(value, sizeof(value), ": %d", integer->value);
				}
				if (const auto* number = _world.get<DevmenuFloatComponent>(_item))
				{
					snprintf(value, sizeof(value), ": %.*f", int(min(number->precision, uint8_t(8))), number->value);
				}
				if (_state.editing == _item && (uint32_t(_time.elapsed * 3.0) & 1)) { value[0] = '\0'; }
				if (const auto* readout = _world.get<DevmenuReadoutComponent>(_item))
				{
					snprintf(value, sizeof(value), ": %s", readout->text.getCPtr());
				}
			}
			snprintf(buffer, sizeof(buffer), "%c%s%s%s", _selected ? '>' : ' ', label, suffix, value);
			text.text.set(buffer);
		}

		static void draw(World& _world, const DevmenuDesc& _menu, DevmenuState& _state,
			const DisplayResource& _display, const TimeResource& _time)
		{
			UiCanvasComponent& canvas = *_world.getMut<UiCanvasComponent>(_state.canvas);
			canvas.visible = _menu.visible;
			canvas.scale = _menu.scale;
			if (!_menu.visible) { return; }
			FROST_ASSERT(_menu.scale > 0 && _menu.textHeight > 0 && _menu.width > 12
				&& _menu.repeatDelay >= 0 && _menu.repeatInterval > 0, "Invalid Devmenu dimensions or repeat timing.");
			_state.count = 0;
			devmenuAppend(_world, _state.current, _state);
			if (_state.current == _menu.root) { devmenuAppend(_world, _menu.custom, _state); }
			DevmenuPageComponent& page = devmenuPage(_world, _state.current);
			const bool back = _state.current != _menu.root;
			const float rowHeight = _menu.textHeight + 2;
			const uint32_t capacity = uint32_t(max(1.0f, floor((float(_display.height) / _menu.scale
				- _menu.y - 12 - rowHeight - 3) / rowHeight)));
			const uint32_t visible = min(_state.count, capacity - uint32_t(back && capacity > 1));
			if (page.selection >= 0)
			{
				if (uint32_t(page.selection) < page.first) { page.first = uint32_t(page.selection); }
				if (uint32_t(page.selection) >= page.first + visible)
				{
					page.first = uint32_t(page.selection) + 1 - visible;
				}
			}
			page.first = min(page.first, _state.count - visible);
			const uint32_t rows = visible + uint32_t(back);
			while (_state.rowCount < rows)
			{
				if (_state.rowCount == _state.rows.getCapacity())
				{
					_state.rows.setCapacity(max(16u, _state.rowCount * 2), _state.rowCount);
				}
				*_state.rows.getPtr(_state.rowCount++) = _world.spawn(UiNodeComponent{}, UiTextComponent{})
					.setParent(_state.list).id();
			}
			for (uint32_t ii = 0; ii < _state.rowCount; ++ii)
			{
				const Entity entity = *_state.rows.getPtr(ii);
				UiNodeComponent& node = *_world.getMut<UiNodeComponent>(entity);
				node.visible = ii < rows;
				node.height = _menu.textHeight;
				node.order = int32_t(ii);
				if (ii >= rows) { continue; }
				const bool backRow = back && 0 == ii;
				const uint32_t index = page.first + ii - uint32_t(back && !backRow);
				row(_world, entity, backRow ? Entity{} : _state.entries.getPtr(index)->entity, backRow,
					backRow ? page.selection == -1 : page.selection == int32_t(index), _menu, _state, _time);
			}
			UiNodeComponent& panel = *_world.getMut<UiNodeComponent>(_state.panel);
			panel.x = _menu.x; panel.y = _menu.y; panel.width = _menu.width; panel.clip = true;
			_world.getMut<UiTextComponent>(_state.title)->height = _menu.textHeight;
			UiNodeComponent& separator = *_world.getMut<UiNodeComponent>(_state.separator);
			separator.width = _menu.width - 12; separator.height = 1;
		}

		static void update(World& _world, ResMutT<DevmenuDesc> _menu, ResMutT<DevmenuState> _state,
			ResT<TimeResource> _time, ResT<InputResource> _input, ResT<DisplayResource> _display)
		{
			refresh(_world, *_time, *_input, *_display, *_state);
			input(_world, *_menu, *_state, *_input, *_time);
			draw(_world, *_menu, *_state, *_display, *_time);
		}

		static void shutdown(World& _world, ResMutT<DevmenuDesc> _menu, ResMutT<DevmenuState> _state)
		{
			devmenuCancel(_world, *_state);
			_world.despawn(_state->canvas);
			_world.despawn(_menu->root);
			_menu->root = {}; _menu->engine = {}; _menu->device = {}; _menu->graphics = {};
		}
	};

	void DevmenuPlugin::build(App& _app)
	{
		_app.getWorld().initResource<DevmenuDesc>();
		_app.getWorld().initResource<DevmenuState>();
		_app.addSystems(EcsStage::Startup, DevmenuSystem::startup);
		_app.addSystems(EcsStage::PreUpdate, DevmenuSystem::update);
		_app.addSystems(EcsStage::PreShutdown, DevmenuSystem::shutdown);
	}
}
