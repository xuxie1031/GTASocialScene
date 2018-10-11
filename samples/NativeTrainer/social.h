#pragma once

#include "../../inc/natives.h"
#include "../../inc/types.h"
#include "../../inc/enums.h"

#include <utility>

typedef struct boundingbox {
	std::pair<float, float> xlim;
	std::pair<float, float> ylim;
	std::pair<float, float> zlim;
}BBX;