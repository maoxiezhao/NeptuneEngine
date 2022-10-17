#pragma once

#include "content\resources\texture.h"
#include "content\resources\model.h"

namespace VulkanTest
{
namespace Editor
{
	class ModelTool
	{
	public:
		static void ComputeVertexTangents(Array<F32x4>& out, I32 indexCount, const U32* indices, const F32x3* vertices, const F32x3* normals, const F32x2* uvs);
		static I32 DetectLodIndex(const char* name);
	};
}
}
