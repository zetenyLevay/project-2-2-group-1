//
// Created by levay on 3/23/2026.
//

#ifndef PROJECT_2_2_GROUP_1_UI_H
#define PROJECT_2_2_GROUP_1_UI_H

#pragma once
#include "imgui.h"
#include "main.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>

void startGui(Grid& grid, Grid& gridTemp, std::vector<double>& temperatures);

#endif //PROJECT_2_2_GROUP_1_UI_H