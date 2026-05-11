#pragma once

#include "local/LocalEngine.h"
#include <string>

std::thread runSimulations(int width, int height, int temperature, bool constantHeatSource, int NumberOfSims, const std::string& filename);