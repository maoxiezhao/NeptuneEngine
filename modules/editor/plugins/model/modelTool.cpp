#include "modelTool.h"
#include "math\vMath_impl.hpp"

namespace VulkanTest
{
namespace Editor
{
	void ModelTool::ComputeVertexTangents(Array<F32x4>& out, I32 indexCount, const U32* indices, const F32x3* vertices, const F32x3* normals, const F32x2* uvs)
	{
		memset(out.data(), 0, sizeof(F32x4) * out.size());
		for (int i = 0; i < indexCount; i += 3)
		{
			const uint32_t i0 = indices[i + 0];
			const uint32_t i1 = indices[i + 1];
			const uint32_t i2 = indices[i + 2];

			const F32x3 v0 = vertices[i0];
			const F32x3 v1 = vertices[i1];
			const F32x3 v2 = vertices[i2];
			const F32x2 uv0 = uvs[i0];
			const F32x2 uv1 = uvs[i1];
			const F32x2 uv2 = uvs[i2];

			const F32x3 n0 = normals[i0];
			const F32x3 n1 = normals[i1];
			const F32x3 n2 = normals[i2];
			const VECTOR facenormal = Vector3Normalize(LoadF32x3(n0 + n1 + n2));

			const float x1 = v1.x - v0.x;
			const float x2 = v2.x - v0.x;
			const float y1 = v1.y - v0.y;
			const float y2 = v2.y - v0.y;
			const float z1 = v1.z - v0.z;
			const float z2 = v2.z - v0.z;

			const float s1 = uv1.x - uv0.x;
			const float s2 = uv2.x - uv0.x;
			const float t1 = uv1.y - uv0.y;
			const float t2 = uv2.y - uv0.y;

			const float r = 1.0f / (s1 * t2 - s2 * t1);
			const VECTOR sdir = VectorSet((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r, 0);
			const VECTOR tdir = VectorSet((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r, 0);

			VECTOR tangent;
			tangent = Vector3Normalize(VectorSubtract(sdir, VectorMultiply(facenormal, Vector3Dot(facenormal, sdir))));
			float sign = VectorGetX(Vector3Dot(Vector3Cross(tangent, facenormal), tdir)) < 0.0f ? -1.0f : 1.0f;

			F32x3 t = StoreF32x3(tangent);
			out[i0].x += t.x;
			out[i0].y += t.y;
			out[i0].z += t.z;
			out[i0].w = sign;

			out[i1].x += t.x;
			out[i1].y += t.y;
			out[i1].z += t.z;
			out[i1].w = sign;

			out[i2].x += t.x;
			out[i2].y += t.y;
			out[i2].z += t.z;
			out[i2].w = sign;
		}	
	}

	I32 ModelTool::DetectLodIndex(const char* name)
	{
		I32 pos = ReverseFindSubstring(name, "LOD");
		if (pos == -1)
			return 0;

		const char* lodStr = name + pos + StringLength("LOD");
		I32 lodIndex;
		FromCString(Span(lodStr, StringLength(lodStr)), lodIndex);
		return lodIndex;
	}
}
}