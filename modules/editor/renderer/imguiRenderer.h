#pragma once

#include "client\app\app.h"
#include "core\platform\platform.h"
#include "gpu\vulkan\device.h"
#include "editor\settings.h"

struct ImFont;

namespace VulkanTest
{
namespace ImGuiRenderer
{
	void CreateContext();
	void Initialize(App& app, Editor::Settings& settings);
	void Uninitialize();
	void BeginFrame();
	void EndFrame();
	void Render();
}
}