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
	struct DevmenuState
	{
		DevmenuState() = default;
		DevmenuState(DevmenuState&& _other)
			: canvas(_other.canvas), panel(_other.panel), title(_other.title), separator(_other.separator), list(_other.list)
			, current(_other.current), editing(_other.editing), editingTarget(_other.editingTarget)
			, count(_other.count), rowCount(_other.rowCount), original(_other.original)
			, repeatAt(_other.repeatAt), repeatDirection(_other.repeatDirection)
		{
			entries = frost::move(_other.entries);
			rows = frost::move(_other.rows);
		}

		Entity canvas, panel, title, separator, list;
		Entity current, editing, editingTarget;
		DynamicStorageT<DevmenuEntry> entries;
		DynamicStorageT<Entity> rows;
		uint32_t count = 0, rowCount = 0;
		double original = 0.0;
		double repeatAt = 0.0;
		int32_t repeatDirection = 0;
	};

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

	static Entity graphicsFolder(World& _world, Entity _parent, const char* _label, int32_t _order)
	{
		return _world.spawn(DevmenuItemComponent{ _label, _order }, DevmenuFolderComponent{}).setParent(_parent).id();
	}

	template<auto MemberT, typename ControlT = DevmenuBoolComponent>
	static void graphicsItem(World& _world, Entity _parent, const char* _label, int32_t _order, ControlT _control = {})
	{
		_world.spawn(DevmenuItemComponent{ _label, _order }, _control, devmenuResource<MemberT>()).setParent(_parent);
	}

	template<auto GroupT, auto MemberT, typename ControlT>
	static void graphicsItem(World& _world, Entity _parent, const char* _label, int32_t _order, ControlT _control)
	{
		using Value = typename detail::DevmenuMemberT<decltype(MemberT)>::Value;
		const DevmenuBindingComponent binding = { {},
			[](const World& _world, Entity) { return double((_world.getResource<GraphicsSettings>()->*GroupT).*MemberT); },
			[](World& _world, Entity, double _value) { (_world.getResourceMut<GraphicsSettings>()->*GroupT).*MemberT = Value(_value); },
			[](const World& _world, Entity) { return NULL != _world.getResource<GraphicsSettings>(); }
		};
		_world.spawn(DevmenuItemComponent{ _label, _order }, _control, binding).setParent(_parent);
	}

	template<GraphicsDebugView::Enum ViewT>
	static void graphicsView(World& _world, Entity _parent, const char* _label, int32_t _order)
	{
		const DevmenuBindingComponent binding = { {},
			[](const World& _world, Entity) { return double(ViewT == _world.getResource<GraphicsSettings>()->debugView); },
			[](World& _world, Entity, double _value)
			{
				_world.getResourceMut<GraphicsSettings>()->debugView = 0.0 != _value ? ViewT : GraphicsDebugView::Final;
			},
			[](const World& _world, Entity) { return NULL != _world.getResource<GraphicsSettings>(); }
		};
		_world.spawn(DevmenuItemComponent{ _label, _order }, DevmenuBoolComponent{}, binding).setParent(_parent);
	}

	static void graphicsMenu(World& _world, Entity _root)
	{
		const Entity graphics = graphicsFolder(_world, _root, "Graphics", 0);
		const Entity geometry = graphicsFolder(_world, graphics, "Geometry", 0);
		graphicsItem<&GraphicsSettings::instancing>(_world, geometry, "Instancing", 0);
		graphicsItem<&GraphicsSettings::frustumCulling>(_world, geometry, "Frustum culling", 1);
		graphicsItem<&GraphicsSettings::lods>(_world, geometry, "LODs", 2);
		graphicsItem<&GraphicsSettings::lodPixels>(_world, geometry, "LOD pixels", 3, DevmenuFloatComponent{ 0, 8, 512, 8, 0 });
		graphicsItem<&GraphicsSettings::normalMapping>(_world, geometry, "Normal mapping", 4);
		graphicsItem<&GraphicsSettings::specularAntialiasing>(_world, geometry, "Specular antialiasing", 5);

		const Entity lighting = graphicsFolder(_world, graphics, "Lighting", 1);
		graphicsItem<&GraphicsSettings::directionalLights>(_world, lighting, "Directional lights", 0);
		graphicsItem<&GraphicsSettings::localLights>(_world, lighting, "Local lights", 1);
		graphicsItem<&GraphicsSettings::emissiveLighting>(_world, lighting, "Emissive lighting", 2);
		const Entity sun = graphicsFolder(_world, lighting, "Directional shadows", 3);
		graphicsItem<&GraphicsSettings::directionalShadows>(_world, sun, "Enabled", 0);
		graphicsItem<&GraphicsSettings::directionalShadow, &DirectionalShadowSettings::maxLights>(_world, sun, "Shadowed lights", 10, DevmenuIntComponent{ 0, 0, 2, 1 });
		graphicsItem<&GraphicsSettings::directionalShadow, &DirectionalShadowSettings::resolution>(_world, sun, "Resolution", 1, DevmenuIntComponent{ 0, 64, 4096, 64 });
		graphicsItem<&GraphicsSettings::directionalShadow, &DirectionalShadowSettings::distance>(_world, sun, "Distance", 2, DevmenuFloatComponent{ 0, 1, 2000, 5, 0 });
		graphicsItem<&GraphicsSettings::directionalShadow, &DirectionalShadowSettings::distanceScale>(_world, sun, "Distance scale", 3, DevmenuFloatComponent{ 0, 0.1, 4, 0.05, 2 });
		graphicsItem<&GraphicsSettings::directionalShadow, &DirectionalShadowSettings::splitLambda>(_world, sun, "Split lambda", 4, DevmenuFloatComponent{ 0, 0, 1, 0.05, 2 });
		graphicsItem<&GraphicsSettings::directionalShadow, &DirectionalShadowSettings::casterDistance>(_world, sun, "Caster distance", 5, DevmenuFloatComponent{ 0, 0, 2000, 5, 0 });
		graphicsItem<&GraphicsSettings::directionalShadow, &DirectionalShadowSettings::depthBias>(_world, sun, "Depth bias", 6, DevmenuFloatComponent{ 0, 0, 0.05, 0.0005, 4 });
		graphicsItem<&GraphicsSettings::directionalShadow, &DirectionalShadowSettings::normalBias>(_world, sun, "Normal bias", 7, DevmenuFloatComponent{ 0, 0, 5, 0.05, 2 });
		graphicsItem<&GraphicsSettings::directionalShadow, &DirectionalShadowSettings::blend>(_world, sun, "Cascade blend", 8, DevmenuFloatComponent{ 0, 0, 0.3, 0.01, 2 });
		graphicsItem<&GraphicsSettings::directionalShadow, &DirectionalShadowSettings::filterRadius>(_world, sun, "Filter radius", 9, DevmenuFloatComponent{ 0, 0, 4, 0.1, 1 });
		const Entity local = graphicsFolder(_world, lighting, "Local shadows", 4);
		graphicsItem<&GraphicsSettings::localShadows>(_world, local, "Enabled", 0);
		graphicsItem<&GraphicsSettings::localShadow, &LocalShadowResource::resolution>(_world, local, "Resolution", 1, DevmenuIntComponent{ 0, 64, 2048, 64 });
		graphicsItem<&GraphicsSettings::localShadow, &LocalShadowResource::maxLights>(_world, local, "Shadowed lights", 2, DevmenuIntComponent{ 0, 0, 30, 1 });
		graphicsItem<&GraphicsSettings::localShadow, &LocalShadowResource::depthBias>(_world, local, "Depth bias", 3, DevmenuFloatComponent{ 0, 0, 0.05, 0.0005, 4 });
		graphicsItem<&GraphicsSettings::localShadow, &LocalShadowResource::normalBias>(_world, local, "Normal bias", 4, DevmenuFloatComponent{ 0, 0, 0.25, 0.005, 3 });

		const Entity gi = graphicsFolder(_world, graphics, "Global illumination", 2);
		graphicsItem<&GraphicsSettings::globalIllumination>(_world, gi, "Enabled", 0);
		graphicsItem<&GraphicsSettings::diffuseGi>(_world, gi, "Diffuse GI", 1);
		graphicsItem<&GraphicsSettings::glossyGi>(_world, gi, "Glossy GI", 2);
		const Entity diffuse = graphicsFolder(_world, gi, "Diffuse probes", 3);
		graphicsItem<&GraphicsSettings::giRaysPerProbe>(_world, diffuse, "Rays per probe", 0, DevmenuIntComponent{ 0, 32, 256, 16 });
		graphicsItem<&GraphicsSettings::giHysteresis>(_world, diffuse, "History weight", 1, DevmenuFloatComponent{ 0, 0, 0.995, 0.005, 3 });
		graphicsItem<&GraphicsSettings::giNormalBias>(_world, diffuse, "Normal bias", 2, DevmenuFloatComponent{ 0, 0, 1, 0.005, 3 });
		graphicsItem<&GraphicsSettings::giViewBias>(_world, diffuse, "View bias", 3, DevmenuFloatComponent{ 0, 0, 1, 0.005, 3 });
		graphicsItem<&GraphicsSettings::giDistanceSharpness>(_world, diffuse, "Distance sharpness", 4, DevmenuFloatComponent{ 0, 1, 64, 1, 0 });
		const Entity glossy = graphicsFolder(_world, gi, "Glossy reflections", 4);
		graphicsItem<&GraphicsSettings::glossyRayFadeStart>(_world, glossy, "Ray fade start roughness", 0, DevmenuFloatComponent{ 0, 0, 0.99, 0.01, 2 });
		graphicsItem<&GraphicsSettings::glossyRayFadeEnd>(_world, glossy, "Ray fade end roughness", 1, DevmenuFloatComponent{ 0, 0.01, 1, 0.01, 2 });

		const Entity sky = graphicsFolder(_world, graphics, "Sky", 3);
		graphicsItem<&GraphicsSettings::skyBackground>(_world, sky, "Background", 0);
		graphicsItem<&GraphicsSettings::skyLighting>(_world, sky, "Sky lighting", 1);
		graphicsItem<&GraphicsSettings::pauseSky>(_world, sky, "Pause time", 2);
		graphicsItem<&GraphicsSettings::skyTimeScale>(_world, sky, "Time scale", 3, DevmenuFloatComponent{ 0, 0, 100, 0.25, 2 });
		graphicsItem<&GraphicsSettings::skyResolutionScale>(_world, sky, "Capture resolution scale", 4, DevmenuFloatComponent{ 0, 0.125, 1, 0.125, 3 });

		const Entity output = graphicsFolder(_world, graphics, "Output", 4);
		graphicsItem<&GraphicsSettings::exposureBias>(_world, output, "Exposure bias (EV)", 0, DevmenuFloatComponent{ 0, -10, 10, 0.25, 2 });
		graphicsItem<&GraphicsSettings::toneMapping>(_world, output, "Tone mapping", 1);
		graphicsItem<&GraphicsSettings::preventBanding>(_world, output, "Dither", 2);
		graphicsItem<&GraphicsSettings::statistics>(_world, output, "Renderer statistics overlay", 3);

		const Entity debug = graphicsFolder(_world, graphics, "Debug views", 5);
		graphicsView<GraphicsDebugView::Final>(_world, debug, "Final image", 0);
		graphicsItem<&GraphicsSettings::debugDepth>(_world, debug, "Depth range", 1, DevmenuFloatComponent{ 0, 0.1, 2000, 1, 1 });
		graphicsItem<&GraphicsSettings::debugExposure>(_world, debug, "Debug exposure (EV)", 2, DevmenuFloatComponent{ 0, -20, 20, 0.5, 1 });
		graphicsItem<&GraphicsSettings::debugMip>(_world, debug, "Mip level", 3, DevmenuIntComponent{ 0, 0, 12, 1 });
		graphicsItem<&GraphicsSettings::debugShadowLight>(_world, debug, "Directional shadow slot", 10, DevmenuIntComponent{ 0, 0, 1, 1 });
		const Entity buffers = graphicsFolder(_world, debug, "G-buffer", 4);
		graphicsView<GraphicsDebugView::BaseColor>(_world, buffers, "Base color", 0);
		graphicsView<GraphicsDebugView::ShadingNormal>(_world, buffers, "Shading normals", 1);
		graphicsView<GraphicsDebugView::GeometricNormal>(_world, buffers, "Geometric normals", 2);
		graphicsView<GraphicsDebugView::Depth>(_world, buffers, "Linear depth", 3);
		graphicsView<GraphicsDebugView::Metallic>(_world, buffers, "Metallic", 4);
		graphicsView<GraphicsDebugView::Roughness>(_world, buffers, "Filtered roughness", 5);
		graphicsView<GraphicsDebugView::Occlusion>(_world, buffers, "Material occlusion", 6);
		graphicsView<GraphicsDebugView::Emissive>(_world, buffers, "Emissive", 7);
		const Entity terms = graphicsFolder(_world, debug, "Lighting", 5);
		graphicsView<GraphicsDebugView::DirectLighting>(_world, terms, "Direct lighting", 0);
		graphicsView<GraphicsDebugView::DiffuseGi>(_world, terms, "Diffuse GI", 1);
		graphicsView<GraphicsDebugView::GlossyGi>(_world, terms, "Glossy GI", 2);
		graphicsView<GraphicsDebugView::Irradiance>(_world, terms, "Unmodulated irradiance", 3);
		const Entity probes = graphicsFolder(_world, debug, "Diffuse probes", 6);
		graphicsItem<&GraphicsSettings::debugProbes>(_world, probes, "Show probes", -3);
		graphicsItem<&GraphicsSettings::debugProbeRadius>(_world, probes, "Probe radius", -2, DevmenuFloatComponent{ 0, 0.025, 5, 0.025, 3 });
		graphicsItem<&GraphicsSettings::debugProbeDepthTest>(_world, probes, "Depth test", -1);
		graphicsView<GraphicsDebugView::ProbeIrradiance>(_world, probes, "Irradiance atlas", 0);
		graphicsView<GraphicsDebugView::ProbeDistance>(_world, probes, "Distance atlas", 1);
		graphicsView<GraphicsDebugView::ProbeVariance>(_world, probes, "Distance standard deviation", 2);
		const Entity reflections = graphicsFolder(_world, debug, "Glossy", 7);
		graphicsView<GraphicsDebugView::GlossyRays>(_world, reflections, "Mirror ray radiance", 0);
		graphicsView<GraphicsDebugView::GlossyFiltered>(_world, reflections, "Filtered radiance", 1);
		graphicsView<GraphicsDebugView::GlossyCoverage>(_world, reflections, "Coverage", 2);
		const Entity shadows = graphicsFolder(_world, debug, "Shadows", 8);
		graphicsView<GraphicsDebugView::DirectionalShadows>(_world, shadows, "Directional shadow atlas", 0);
		graphicsView<GraphicsDebugView::LocalShadows>(_world, shadows, "Local shadow atlas", 1);
		graphicsView<GraphicsDebugView::ShadowCascades>(_world, shadows, "Cascade selection", 2);
		const Entity radiance = graphicsFolder(_world, debug, "Sky", 9);
		graphicsView<GraphicsDebugView::SkyRadiance>(_world, radiance, "Sky radiance", 0);
	}

	struct DevmenuSystem
	{
		static void startup(World& _world, ResMutT<DevmenuDesc> _menu, ResMutT<DevmenuState> _state)
		{
			_state->entries.setAllocator(_world.getAllocator());
			_state->rows.setAllocator(_world.getAllocator());
			_menu->root = _world.spawn(DevmenuFolderComponent{}, DevmenuPageComponent{}).id();
			_state->current = _menu->root;

			_state->canvas = _world.spawn(UiCanvasComponent{ _menu->scale, 1000, _menu->visible }).id();
			_state->panel = _world.spawn(UiNodeComponent{}, UiLayoutComponent{ UiLayoutComponent::Column, 6, 2 },
				UiRectComponent{ 0x383838ef, 0xb0b0b0ff, 1 }).setParent(_state->canvas).id();
			_state->title = _world.spawn(UiNodeComponent{}, UiTextComponent{ "Dev Menu", 0x00ffffff })
				.setParent(_state->panel).id();
			_state->separator = _world.spawn(UiNodeComponent{ .order = 1 }, UiRectComponent{ 0x808080ff })
				.setParent(_state->panel).id();
			_state->list = _world.spawn(UiNodeComponent{ .order = 2 }, UiLayoutComponent{ UiLayoutComponent::Column, 0, 2 })
				.setParent(_state->panel).id();
			graphicsMenu(_world, _menu->root);
		}

		static void refresh(World& _world)
		{
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
			refresh(_world);
			input(_world, *_menu, *_state, *_input, *_time);
			refresh(_world); // One setting may change several selection indicators.
			draw(_world, *_menu, *_state, *_display, *_time);
		}

		static void shutdown(World& _world, ResMutT<DevmenuDesc> _menu, ResMutT<DevmenuState> _state)
		{
			devmenuCancel(_world, *_state);
			_world.despawn(_state->canvas);
			_world.despawn(_menu->root);
			_menu->root = {};
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
