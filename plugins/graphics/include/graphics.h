/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#ifndef BX_PLUGIN_GRAPHICS_H_HEADER_GUARD
#define BX_PLUGIN_GRAPHICS_H_HEADER_GUARD

#include <frost/ecs.h>

namespace frost
{
	struct GraphicsResource
	{
		/// Initial render width in pixels.
		///
		uint32_t width = 1280;

		/// Initial render height in pixels.
		///
		uint32_t height = 720;
	};

	struct GraphicsPlugin : PluginI
	{
		virtual void build(App& _app) override;
	};

} // namespace frost

#endif // BX_PLUGIN_GRAPHICS_H_HEADER_GUARD
