#pragma once

#include "../../inc/natives.h"
#include "../../inc/enums.h"
#include "../../inc/types.h"

#include <utility>
#include <random>

#define MAXWAITPEDNUM 6

typedef struct boundingbox {
	std::pair<float, float> xlim;
	std::pair<float, float> ylim;
	std::pair<float, float> zlim;
}BBX;