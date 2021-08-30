#pragma once

#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <assert.h>
#include <math.h>

#ifdef _MSC_VER
#pragma warning( disable :26812)
#define COMPILER_MSVC 1
#define NOEXCEPT
#else
#define COMPILER_MSVC 0
#endif