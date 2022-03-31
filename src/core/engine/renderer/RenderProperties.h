
#ifndef WORLDENGINE_RENDERPROPERTIES_H
#define WORLDENGINE_RENDERPROPERTIES_H

#include "core/core.h"

struct RenderProperties {

	enum {
		AnimationType_Static = 0, // The transform of the entity never changes.
		AnimationType_Dynamic = 1, // The transform of the entity chanegs sometimes.
		AnimationType_Animated = 2, // The transform of the entity changes every frame.
	};



	union {
		struct {
			uint8_t animationType : 2;
		};
		uint32_t _data;
	};
};


#endif //WORLDENGINE_RENDERPROPERTIES_H
