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
	///
	struct DevmenuItemComponent
	{
		FixedString64 label;
		int32_t order = 0;
		bool visible = true;
	};

	///
	struct DevmenuFolderComponent
	{
	};

	///
	struct DevmenuButtonComponent
	{
		bool activated = false;
		bool pressed = false;
	};

	///
	struct DevmenuBoolComponent
	{
		bool value = false;
		bool readOnly = false;
	};

	///
	struct DevmenuIntComponent
	{
		int32_t value = 0;
		int32_t minimum = INT32_MIN;
		int32_t maximum = INT32_MAX;
		int32_t step = 1;
		bool readOnly = false;
	};

	///
	struct DevmenuFloatComponent
	{
		double value = 0.0;
		double minimum = -1.0e30;
		double maximum = 1.0e30;
		double step = 0.1;
		uint8_t precision = 2;
		bool readOnly = false;
	};

	/// TODO: Is this really needed??
	struct DevmenuBindingComponent
	{
		Entity target;
		double (*read)(const World&, Entity) = NULL;
		void (*write)(World&, Entity, double) = NULL;
		bool (*available)(const World&, Entity) = NULL;
	};

	// TODO: I dont know about this...
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

	///
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

	/// 
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

} // namespace frost

namespace frost
{
	/// Const device configuration read on startup.
	///
	struct DevmenuDesc
	{
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

		Entity root;     //!< Populated during Startup. Applications may also parent items here.
		Entity custom;   //!< Applications may also parent items here.
	};

	/// Runtime devmenu configuration read every frame.
	///
	/// @remark Available after plugin is built.
	///
	struct DevmenuSettings
	{
	};

	/// Creates the devmenu.
	///
	/// @remark Requires `DevicePlugin` and `GraphicsPlugin`.
	/// @remark Insert `DevmenuDesc` before plugin is added to application.
	///
	struct DevmenuPlugin : PluginI
	{
		virtual void build(App& _app) override;
	};

} // namespace frost

#endif // FROST_PLUGIN_DEVMENU_H_HEADER_GUARD
