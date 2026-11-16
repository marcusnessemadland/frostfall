/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_PLUGIN_DEVMENU_H_HEADER_GUARD
#define FROST_PLUGIN_DEVMENU_H_HEADER_GUARD

#include <frost/ecs.h>
#include <frost/string.h>
#include <device.h>

namespace frost
{
	/// Parent items with World::setParent. Siblings sort by order, then entity ID.
	struct DevmenuItemComponent
	{
		FixedString64 label;
		int32_t order = 0;
		bool visible = true;
	};

	struct DevmenuFolderComponent {};

	/// activated is true for one update on the select press; pressed lasts until release.
	/// Application Update systems can read these fields to implement actions.
	struct DevmenuButtonComponent
	{
		bool activated = false;
		bool pressed = false;
	};

	struct DevmenuBoolComponent
	{
		bool value = false;
		bool readOnly = false;
	};

	struct DevmenuIntComponent
	{
		int32_t value = 0;
		int32_t minimum = INT32_MIN;
		int32_t maximum = INT32_MAX;
		int32_t step = 1;
		bool readOnly = false;
	};

	struct DevmenuFloatComponent
	{
		double value = 0.0;
		double minimum = -1.0e30;
		double maximum = 1.0e30;
		double step = 0.1;
		uint8_t precision = 2;
		bool readOnly = false;
	};

	struct DevmenuReadoutComponent
	{
		FixedString256 text;
	};

	/// Optional live binding. Without one, the item's value component is the data.
	/// Bindings resolve their target on each access, never retain ECS storage pointers.
	/// A missing component/resource temporarily hides the item. Custom bindings may
	/// supply these functions directly. Keep them free of structural ECS changes.
	struct DevmenuBindingComponent
	{
		Entity target;
		double (*read)(const World&, Entity) = NULL;
		void (*write)(World&, Entity, double) = NULL;
		bool (*available)(const World&, Entity) = NULL;
	};

	namespace detail
	{
		template<typename Ty> struct DevmenuMemberT;
		template<typename ValueT, typename OwnerT>
		struct DevmenuMemberT<ValueT OwnerT::*>
		{
			using Owner = OwnerT;
			using Value = ValueT;
		};
	}

	/// Bind an integer, floating-point, enum or bool component member.
	template<auto MemberT>
	DevmenuBindingComponent devmenuComponent(Entity _entity)
	{
		using Member = detail::DevmenuMemberT<decltype(MemberT)>;
		using Owner = typename Member::Owner;
		using Value = typename Member::Value;
		return { _entity,
			[](const World& _world, Entity _target) { return double(_world.get<Owner>(_target)->*MemberT); },
			[](World& _world, Entity _target, double _value) { _world.getMut<Owner>(_target)->*MemberT = Value(_value); },
			[](const World& _world, Entity _target) { return _world.has<Owner>(_target); }
		};
	}

	/// Bind a public resource member. The resource must be present while the item is used.
	template<auto MemberT>
	DevmenuBindingComponent devmenuResource()
	{
		using Member = detail::DevmenuMemberT<decltype(MemberT)>;
		using Owner = typename Member::Owner;
		using Value = typename Member::Value;
		return { {},
			[](const World& _world, Entity) { return double(_world.getResource<Owner>()->*MemberT); },
			[](World& _world, Entity, double _value) { _world.getResourceMut<Owner>()->*MemberT = Value(_value); },
			[](const World& _world, Entity) { return NULL != _world.getResource<Owner>(); }
		};
	}

	struct DevmenuResource
	{
		/// Optional application-owned container. Its children appear beside the built-in
		/// folders at the menu root. Configure here, or replace it at runtime.
		/// Devmenu never reparents or destroys this hierarchy.
		Entity custom;

		GamepadButton::Enum toggle = GamepadButton::Start;
		GamepadButton::Enum up = GamepadButton::DpadUp;
		GamepadButton::Enum down = GamepadButton::DpadDown;
		GamepadButton::Enum select = GamepadButton::South;
		GamepadButton::Enum back = GamepadButton::East;
		float repeatDelay = 0.35f;
		float repeatInterval = 0.08f;
		float x = 8.0f;
		float y = 8.0f;
		float width = 448.0f;
		float textHeight = 16.0f;
		float scale = 1.0f;
		bool visible = false;

		/// Populated during Startup. Applications may also parent items here.
		Entity root;
		Entity engine;
		Entity device;
		Entity graphics;
	};

	/// Requires DevicePlugin and GraphicsPlugin. Draws through graphics UI components.
	/// Never consumes gameplay input or pauses simulation. Select commits numeric edits;
	/// Back restores the original value. Closing the menu cancels an unfinished edit.
	struct DevmenuPlugin : PluginI
	{
		virtual void build(App& _app) override;
	};

} // namespace frost

#endif // FROST_PLUGIN_DEVMENU_H_HEADER_GUARD
