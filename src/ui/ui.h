#pragma once

#define GL_SILENCE_DEPRECATION

#include "imgui.h"
#include "main.h"
#include "../data/local/LocalEngine.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <vector>

void launchGui();

void startGui(DataSource source);