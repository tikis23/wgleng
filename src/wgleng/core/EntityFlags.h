#pragma once

#include <stdint.h>

using entityFlagType = uint32_t;
namespace EntityFlags {
	enum EntityFlagsEnum: entityFlagType {
		NONE			   = 0 << 0,
		PICKABLE		   = 1 << 0,
		INTERACTABLE	   = 1 << 1,
		// If you change this after the body was created, you need to update the body manually too
		DISABLE_COLLISIONS = 1 << 2,
		// If you change this after the body was created, you need to update the body manually too
		DISABLE_GRAVITY    = 1 << 3,
		OCCLUDER           = 1 << 4,
	};
}