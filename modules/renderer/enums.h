#pragma once

namespace VulkanTest
{
	enum RasterizerStateTypes
	{
		RSTYPE_BACK,
		RSTYPE_FRONT,
		RSTYPE_DOUBLE_SIDED,
		RSTYPE_SKY,
		RSTYPE_COUNT
	};

	enum BlendStateTypes
	{
		BSTYPE_OPAQUE,
		BSTYPE_TRANSPARENT,
		BSTYPE_PREMULTIPLIED,
		BSTYPE_COUNT
	};

	enum DepthStencilStateType
	{
		DSTYPE_DISABLED,
		DSTYPE_DEFAULT,
		DSTYPE_READEQUAL,
		DSTYPE_READ,
		DSTYPE_COUNT
	};

	enum SamplerType
	{
		SAMPLERTYPE_OBJECT,

		SAMPLERTYPE_COUNT
	};

	enum ShaderType
	{
		SHADERTYPE_OBJECT,
		SHADERTYPE_VERTEXCOLOR,
		SHADERTYPE_VISIBILITY,
		SHADERTYPE_MESHLET,
		SHADERTYPE_POSTPROCESS_OUTLINE,
		SHADERTYPE_POSTPROCESS_BLUR_GAUSSIAN,
		SHADERTYPE_TILED_LIGHT_CULLING,
		SHADERTYPE_SKY,
		SHADERTYPE_TONEMAP,
		SHADERTYPE_FXAA,

		SHADERTYPE_COUNT
	};

	enum RENDERPASS
	{
		RENDERPASS_MAIN = 1 << 0,
		RENDERPASS_PREPASS = 1 << 1,
		RENDERPASS_SHADOE = 1 << 2,
	};

	enum BlendMode
	{
		BLENDMODE_OPAQUE,
		BLENDMODE_ALPHA,
		BLENDMODE_PREMULTIPLIED,
		BLENDMODE_COUNT
	};

	enum ObjectDoubleSided
	{
		OBJECT_DOUBLESIDED_FRONTSIDE,
		OBJECT_DOUBLESIDED_ENABLED,
		OBJECT_DOUBLESIDED_BACKSIDE,
		OBJECT_DOUBLESIDED_COUNT
	};
}