#pragma once
#include "imgui.h"
#include "main.h"
#include "../data/local/LocalEngine.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>

void launchGui(SimulationEngine& engine);

void startGui(DataSource source);