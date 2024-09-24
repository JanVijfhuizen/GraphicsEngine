#pragma once
#include <cstdint>
#include <iostream>
#include <cassert>
#include <new.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>

#include "JLib/Arena.h"
#include "JLib/Iterator.h"
#include "JLib/KeyPair.h"
#include "windows.h"