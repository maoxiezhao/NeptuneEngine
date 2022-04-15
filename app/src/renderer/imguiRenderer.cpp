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

	struct ImGuiImplData {};

	ImGuiImplData* ImGuiImplGetBackendData()
	{
		return ImGui::GetCurrentContext() ? (ImGuiImplData*)ImGui::GetIO().BackendRendererUserData : nullptr;
	}

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
		ImFont* font = io.Fonts->AddFontFromMemoryTTF((void*)mem.Data(), (I32)mem.Size(), 12.0f, &cfg);
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
		ImGuiImplData* bd = IM_NEW(ImGuiImplData)();
		io.BackendRendererUserData = (void*)bd;
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
		auto backendData = ImGuiImplGetBackendData();
		ASSERT(backendData != nullptr);

		if (!fontTexture)
			CreateDeviceObjects();

#ifdef CJING3D_PLATFORM_WIN32
		ImGui_ImplGlfw_NewFrame();
#endif
		ImGui::NewFrame();
		ImGui::PushFont(font);
	}

	void EndFrame()
	{
		ImGui::PopFont();
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
		cmd->BeginEvent("ImGuiRender");

		//cmd->SetProgram("editor/imGuiVS.hlsl", "editor/imGuiPS.hlsl");

		//ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
		//for (ImGuiViewport* vp : platformIO.Viewports)
		//{
		//	ImDrawData* drawData = vp->DrawData;
		//	if (!drawData || drawData->TotalVtxCount == 0)
		//		continue;

		//	int fbWidth = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
		//	int fbHeight = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
		//	if (fbWidth <= 0 || fbHeight <= 0)
		//		continue;

		//	// Setup vertex buffer and index buffer
		//	cmd->SetVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, (U32)IM_OFFSETOF(ImDrawVert, pos));
		//	cmd->SetVertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, (U32)IM_OFFSETOF(ImDrawVert, uv));
		//	cmd->SetVertexAttribute(2, 0, VK_FORMAT_R8G8B8A8_UNORM, (U32)IM_OFFSETOF(ImDrawVert, col));

		//	const U64 vbSize = sizeof(ImDrawVert) * drawData->TotalVtxCount;
		//	const U64 ibSize = sizeof(ImDrawIdx) * drawData->TotalIdxCount;
		//	ImDrawVert* vertMem = static_cast<ImDrawVert*>(cmd->AllocateVertexBuffer(0, vbSize, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX));
		//	ImDrawIdx* indexMem = static_cast<ImDrawIdx*>(cmd->AllocateIndexBuffer(ibSize, VK_INDEX_TYPE_UINT16));
		//	for (int cmdListIdx = 0; cmdListIdx < drawData->CmdListsCount; cmdListIdx++)
		//	{
		//		const ImDrawList* drawList = drawData->CmdLists[cmdListIdx];
		//		memcpy(vertMem, &drawList->VtxBuffer[0], drawList->VtxBuffer.Size * sizeof(ImDrawVert));
		//		memcpy(indexMem, &drawList->IdxBuffer[0], drawList->IdxBuffer.Size * sizeof(ImDrawIdx));
		//		vertMem += drawList->VtxBuffer.Size;
		//		indexMem += drawList->IdxBuffer.Size;
		//	}
		//
		//	// Setup mvp matrix
		//	const float L = drawData->DisplayPos.x;
		//	const float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
		//	const float T = drawData->DisplayPos.y;
		//	const float B = drawData->DisplayPos.y + drawData->DisplaySize.y;

		//	struct ImGuiConstants
		//	{
		//		float  mvp[4][4];
		//	};
		//	float mvp[4][4] =
		//	{
		//		{ 2.0f / (R - L),    0.0f,                 0.0f,       0.0f },
		//		{ 0.0f,              2.0f / (T - B),       0.0f,       0.0f },
		//		{ 0.0f,              0.0f,                 0.5f,       0.0f },
		//		{ (R + L) / (L - R), (T + B) / (B - T),    0.5f,       1.0f },
		//	};
		//	ImGuiConstants* constants = cmd->AllocateConstant<ImGuiConstants>(0, 0);
		//	memcpy(&constants->mvp, mvp, sizeof(mvp));
		//	
		//	// Set viewport
		//	VkViewport vp = {};
		//	vp.x = 0.0f;
		//	vp.y = 0.0f;
		//	vp.width = (F32)fbWidth;
		//	vp.height = (F32)fbHeight;
		//	vp.minDepth = 0.0f;
		//	vp.maxDepth = 1.0f;
		//	cmd->SetViewport(vp);

		//	// Set states
		//	cmd->SetSampler(0, 0, *sampler);
		//	cmd->SetDefaultTransparentState();
		//	cmd->SetBlendState(Renderer::GetBlendState(BlendStateType_Transparent));
		//	cmd->SetRasterizerState(Renderer::GetRasterizerState(RasterizerStateType_DoubleSided));
		//	// cmd->SetProgram("test/triangleVS.hlsl", "test/trianglePS.hlsl");
		//	cmd->SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		//	// Will project scissor/clipping rectangles into framebuffer space
		//	ImVec2 clipOff = drawData->DisplayPos;         // (0,0) unless using multi-viewports
		//	ImVec2 clipScale = drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		//	// Render command lists
		//	I32 vertexOffset = 0;
		//	U32 indexOffset = 0;
		//	for (U32 cmdListIdx = 0; cmdListIdx < (U32)drawData->CmdListsCount; ++cmdListIdx)
		//	{
		//		const ImDrawList* drawList = drawData->CmdLists[cmdListIdx];
		//		for (U32 cmdIndex = 0; cmdIndex < (U32)drawList->CmdBuffer.size(); ++cmdIndex)
		//		{
		//			const ImDrawCmd* drawCmd = &drawList->CmdBuffer[cmdIndex];
		//			ASSERT(!drawCmd->UserCallback);
		//			
		//			// Project scissor/clipping rectangles into framebuffer space
		//			ImVec2 clipMin(drawCmd->ClipRect.x - clipOff.x, drawCmd->ClipRect.y - clipOff.y);
		//			ImVec2 clipMax(drawCmd->ClipRect.z - clipOff.x, drawCmd->ClipRect.w - clipOff.y);
		//			if (clipMax.x < clipMin.x || clipMax.y < clipMin.y)
		//				continue;

		//			// Apply scissor/clipping rectangle
		//			VkRect2D scissor;
		//			scissor.offset.x = (I32)(clipMin.x);
		//			scissor.offset.y = (I32)(clipMin.y);
		//			scissor.extent.width = (I32)(clipMax.x - clipMin.x);
		//			scissor.extent.height = (I32)(clipMax.y - clipMin.y);
		//			cmd->SetScissor(scissor);

		//			const GPU::Image* texture = (const GPU::Image*)drawCmd->TextureId;
		//			cmd->SetTexture(0, 0, texture->GetImageView());
		//			cmd->DrawIndexed(drawCmd->ElemCount, indexOffset, vertexOffset);
		//		
		//			indexOffset += drawCmd->ElemCount;
		//		}
		//		vertexOffset += drawList->VtxBuffer.size();
		//	}
		//}
		//cmd->EndEvent();

		cmd->SetDefaultTransparentState();
		cmd->SetBlendState(Renderer::GetBlendState(BlendStateType_Transparent));
		cmd->SetRasterizerState(Renderer::GetRasterizerState(RasterizerStateType_DoubleSided));
		cmd->SetProgram("test/blitVS.hlsl", "test/blitPS.hlsl");
		cmd->SetSampler(0, 0, *sampler);
		cmd->SetTexture(0, 0, fontTexture->GetImageView());
		cmd->Draw(3);
	}
}
}