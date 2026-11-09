/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/easing.h>

namespace frost
{
	static const EaseFn s_easeFunc[] =
	{
		easeLinear,
		easeStep,
		easeSmoothStep,
		easeInQuad,
		easeOutQuad,
		easeInOutQuad,
		easeOutInQuad,
		easeInCubic,
		easeOutCubic,
		easeInOutCubic,
		easeOutInCubic,
		easeInQuart,
		easeOutQuart,
		easeInOutQuart,
		easeOutInQuart,
		easeInQuint,
		easeOutQuint,
		easeInOutQuint,
		easeOutInQuint,
		easeInSine,
		easeOutSine,
		easeInOutSine,
		easeOutInSine,
		easeInExpo,
		easeOutExpo,
		easeInOutExpo,
		easeOutInExpo,
		easeInCirc,
		easeOutCirc,
		easeInOutCirc,
		easeOutInCirc,
		easeInElastic,
		easeOutElastic,
		easeInOutElastic,
		easeOutInElastic,
		easeInBack,
		easeOutBack,
		easeInOutBack,
		easeOutInBack,
		easeInBounce,
		easeOutBounce,
		easeInOutBounce,
		easeOutInBounce,
	};
	static_assert(FROST_COUNTOF(s_easeFunc) == Easing::Count);

	EaseFn getEaseFunc(Easing::Enum _enum)
	{
		return s_easeFunc[_enum];
	}

} // namespace frost
