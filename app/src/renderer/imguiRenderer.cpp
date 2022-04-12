#include "imguiRenderer.h"
#include "renderer\renderPath3D.h"
#include "renderer\renderer.h"
#include "core\jobsystem\jobsystem.h"
#include "core\filesystem\filesystem.h"
#include "core\utils\stream.h"

#include "imgui_impl_glfw.h"
#include "glfw\include\GLFW\glfw3.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace ImGuiRenderer
{
	App* app;
	ImFont* font;
	GPU::ImagePtr fontTexture;
	GPU::SamplerPtr sampler;

	void CreateDeviceObjects()
	{
		WSI& wsi = app->GetWSI();
		GPU::DeviceVulkan* device = wsi.GetDevice();

		// Get font texture data
		unsigned char* pixels;
		int width, height;
		ImFontAtlas* atlas = ImGui::GetIO().Fonts;
		atlas->GetTexDataAsRGBA32(&pixels, &width, &height);

		// Create font texture
		GPU::ImageCreateInfo imageInfo = GPU::ImageCreateInfo::ImmutableImage2D(width, height, VK_FORMAT_R8G8B8A8_UNORM);
		GPU::TextureFormatLayout formatLayout;
		formatLayout.SetTexture2D(VK_FORMAT_R8G8B8A8_UNORM, width, height);
		GPU::SubresourceData data = {};
		data.data = pixels;

		fontTexture = device->CreateImage(imageInfo, &data);
		device->SetName(*fontTexture, "ImGuiFontTexture");

		// Create sampler
		GPU::SamplerCreateInfo samplerInfo = {};
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = false;
		samplerInfo.compareEnable = false;

		sampler = device->RequestSampler(samplerInfo, false);
		device->SetName(*sampler, "ImGuiSampler");

		// Store our font texture
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->SetTexID((ImTextureID)&(*fontTexture));
	}

	ImFont* AddFontFromFile(App& app, const char* path)
	{
		Engine& engine = app.GetEngine();
		OutputMemoryStream mem;
		if (!engine.GetFileSystem().LoadContext(path, mem))
		{
			Logger::Error("Failed to load font %s", path);
			return nullptr;
		}

		ImGuiIO& io = ImGui::GetIO();
		ImFontConfig cfg;
		CopyString(cfg.Name, path);
		cfg.FontDataOwnedByAtlas = false;
		ImFont* font = io.Fonts->AddFontFromMemoryTTF((void*)mem.Data(), mem.Size(), 12, &cfg);
		ASSERT(font != NULL);
		return nullptr;
	}

	void Initialize(App& app_)
	{
		Logger::Info("Initializing imgui...");
		app = &app_;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		// Setup context
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= 
			ImGuiConfigFlags_DockingEnable | 
			ImGuiConfigFlags_ViewportsEnable;

		// Setup style
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// Add fonts
		font = AddFontFromFile(*app, "editor/fonts/notosans-regular.ttf");

		// Setup platform
#ifdef CJING3D_PLATFORM_WIN32
		GLFWwindow* window = static_cast<GLFWwindow*>(app->GetPlatform().GetWindow());
		ImGui_ImplGlfw_InitForVulkan(window, true);
#endif
		// Setup renderer backend
		io.BackendRendererName = "VulkanTest";
		io.BackendFlags = 
			ImGuiBackendFlags_PlatformHasViewports | 
			ImGuiBackendFlags_RendererHasViewports | 
			ImGuiBackendFlags_HasMouseCursors;
	}

	void Uninitialize()
	{
		sampler.reset();
		fontTexture.reset();

#ifdef CJING3D_PLATFORM_WIN32
		ImGui_ImplGlfw_Shutdown();
#endif
		ImGui::DestroyContext();
		app = nullptr;
	}

	void BeginFrame()
	{
		if (!fontTexture)
			CreateDeviceObjects();

#ifdef CJING3D_PLATFORM_WIN32
		ImGui_ImplGlfw_NewFrame();
#endif
		ImGui::NewFrame();
	}

	void EndFrame()
	{
		ImGui::Render();

		// Update and Render additional Platform Windows
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void Render(GPU::CommandList* cmd)
	{
		cmd->SetDefaultTransparentState();
		cmd->SetBlendState(Renderer::GetBlendState(BlendStateType_Transparent));
		cmd->SetRasterizerState(Renderer::GetRasterizerState(RasterizerStateType_DoubleSided));
		cmd->SetProgram("test/blitVS.hlsl", "test/blitPS.hlsl");
		cmd->SetSampler(0, 0, *sampler);
		cmd->SetTexture(1, 0, fontTexture->GetImageView());
		cmd->Draw(3);
	}
}
}