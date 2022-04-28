#pragma once

namespace VulkanTest
{
	enum RasterizerStateTypes
	{
		RasterizerStateType_Back,
		RasterizerStateType_Front,
		RasterizerStateType_DoubleSided,
		RasterizerStateType_Count
	};

	enum BlendStateTypes
	{
		BlendStateType_Opaque,
		BlendStateType_Transparent,
		BlendStateType_Count
	};

	enum RENDERPASS
	{
		RENDERPASS_MAIN,
		RENDERPASS_PREPASS,
		RENDERPASS_SHADOE,
		RENDERPASS_COUNT
	};
}