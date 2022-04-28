#pragma once

#define USE_DIRECTX_MATH
#ifdef USE_DIRECTX_MATH
#if __has_include("DirectXMath.h")
// In this case, DirectXMath is coming from Windows SDK.
//	It is better to use this on Windows as some Windows libraries could depend on the same 
//	DirectXMath headers
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>
#else
// In this case, DirectXMath is coming from supplied source code
//	On platforms that don't have Windows SDK, the source code for DirectXMath is provided
//	as part of the engine utilities
#include "Utility/DirectXMath.h"
#include "Utility/DirectXPackedVector.h"
#include "Utility/DirectXCollision.h"
#endif

#include "vMath.h"

namespace VulkanTest 
{
	using namespace DirectX;
	using VECTOR = XMVECTOR;
	using MATRIX = XMMATRIX;

	const F32 MATH_PI = XM_PI;
	const F32 MATH_2PI = XM_2PI;

	////////////////////////////////////////////////////////////////////////////
	// Type checks
	////////////////////////////////////////////////////////////////////////////

	static_assert(sizeof(I32x2) == sizeof(XMFLOAT2), "DirectXMath type mismatch");
	static_assert(sizeof(I32x3) == sizeof(XMFLOAT3), "DirectXMath type mismatch");
	static_assert(sizeof(I32x4) == sizeof(XMFLOAT4), "DirectXMath type mismatch");

	static_assert(sizeof(F32x2) == sizeof(XMFLOAT2), "DirectXMath type mismatch");
	static_assert(sizeof(F32x3) == sizeof(XMFLOAT3), "DirectXMath type mismatch");
	static_assert(sizeof(F32x4) == sizeof(XMFLOAT4), "DirectXMath type mismatch");

	static_assert(sizeof(U32x2) == sizeof(XMUINT2), "DirectXMath type mismatch");
	static_assert(sizeof(U32x3) == sizeof(XMUINT3), "DirectXMath type mismatch");
	static_assert(sizeof(U32x4) == sizeof(XMUINT4), "DirectXMath type mismatch");

	////////////////////////////////////////////////////////////////////////////
	// Matrix function
	////////////////////////////////////////////////////////////////////////////
	
	inline MATRIX XM_CALLCONV MatrixMultiply(MATRIX m1, MATRIX m2)
	{
		return XMMatrixMultiply(m1, m2);
	}

	inline MATRIX XM_CALLCONV MatrixPerspectiveFovLH
	(
		F32 FovAngleY,
		F32 AspectRatio,
		F32 NearZ,
		F32 FarZ
	)
	{
		return XMMatrixPerspectiveFovLH(FovAngleY, AspectRatio, NearZ, FarZ);
	}

	inline MATRIX XM_CALLCONV MatrixLookToLH
	(
		VECTOR EyePosition,
		VECTOR EyeDirection,
		VECTOR UpDirection
	)
	{
		return XMMatrixLookToLH(EyePosition, EyeDirection, UpDirection);
	}

	inline MATRIX XM_CALLCONV MatrixInverse(MATRIX M)
	{
		return XMMatrixInverse(nullptr, M);
	}	

	////////////////////////////////////////////////////////////////////////////
	// Load
	////////////////////////////////////////////////////////////////////////////

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadF32(const F32& src) noexcept {
		return XMLoadFloat(&src);
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadF32x2(const F32x2& src) noexcept {
		return XMLoadFloat2(reinterpret_cast<const XMFLOAT2*>(&src));
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadF32x3(const F32x3& src) noexcept {
		return XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&src));
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadF32x4(const F32x4& src) noexcept {
		return XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(&src));
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadI32(const I32& src) noexcept {
		VECTOR result = XMVectorZero();
		result = XMVectorSetX(result, (F32)src);
		return result;
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadI32x2(const I32x2& src) noexcept {
		return XMLoadSInt2(reinterpret_cast<const XMINT2*>(&src));
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadI32x3(const I32x3& src) noexcept {
		return XMLoadSInt3(reinterpret_cast<const XMINT3*>(&src));
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadI32x4(const I32x4& src) noexcept {
		return XMLoadSInt4(reinterpret_cast<const XMINT4*>(&src));
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadU32(const U32& src) noexcept {
		VECTOR result = XMVectorZero();
		result = XMVectorSetX(result, (F32)src);
		return result;
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadU32x2(const U32x2& src) noexcept {
		return XMLoadUInt2(reinterpret_cast<const XMUINT2*>(&src));
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadU32x3(const U32x3& src) noexcept {
		return XMLoadUInt3(reinterpret_cast<const XMUINT3*>(&src));
	}

	CJING_FORCE_INLINE const VECTOR XM_CALLCONV LoadU32x4(const U32x4& src) noexcept {
		return XMLoadUInt4(reinterpret_cast<const XMUINT4*>(&src));
	}

	CJING_FORCE_INLINE const MATRIX XM_CALLCONV LoadFMat3x3(const FMat3x3& src) noexcept {
		return XMLoadFloat3x3(reinterpret_cast<const XMFLOAT3X3*>(&src));
	}

	CJING_FORCE_INLINE const MATRIX XM_CALLCONV LoadFMat4x4(const FMat4x4& src) noexcept {
		return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&src));
	}

	////////////////////////////////////////////////////////////////////////////
	// Store
	////////////////////////////////////////////////////////////////////////////
	
	CJING_FORCE_INLINE const F32 XM_CALLCONV StoreF32(VECTOR src) noexcept {
		F32 dst;
		XMStoreFloat(&dst, src);
		return dst;
	}

	CJING_FORCE_INLINE const F32x2 XM_CALLCONV StoreF32x2(VECTOR src) noexcept {
		F32x2 dst;
		XMStoreFloat2(reinterpret_cast<XMFLOAT2*>(&dst), src);
		return dst;
	}

	CJING_FORCE_INLINE const F32x3 XM_CALLCONV StoreF32x3(VECTOR src) noexcept {
		F32x3 dst;
		XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&dst), src);
		return dst;
	}

	CJING_FORCE_INLINE const F32x4 XM_CALLCONV StoreF32x4(VECTOR src) noexcept {
		F32x4 dst;
		XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&dst), src);
		return dst;
	}
	
	CJING_FORCE_INLINE const I32 XM_CALLCONV StoreI32(VECTOR src) noexcept {
		return (I32)XMVectorGetX(src);
	}

	CJING_FORCE_INLINE const I32x2 XM_CALLCONV StoreI32x2(VECTOR src) noexcept {
		I32x2 dst;
		XMStoreSInt2(reinterpret_cast<XMINT2*>(&dst), src);
		return dst;
	}
	
	CJING_FORCE_INLINE const I32x3 XM_CALLCONV StoreI32x3(VECTOR src) noexcept {
		I32x3 dst;
		XMStoreSInt3(reinterpret_cast<XMINT3*>(&dst), src);
		return dst;
	}
	
	CJING_FORCE_INLINE const I32x4 XM_CALLCONV StoreI32x4(VECTOR src) noexcept {
		I32x4 dst;
		XMStoreSInt4(reinterpret_cast<XMINT4*>(&dst), src);
		return dst;
	}
	
	CJING_FORCE_INLINE const U32 XM_CALLCONV StoreU32(VECTOR src) noexcept {
		U32 dst;
		XMStoreInt(&dst, src);
		return dst;
	}
	
	CJING_FORCE_INLINE const U32x2 XM_CALLCONV StoreU32x2(VECTOR src) noexcept {
		U32x2 dst;
		XMStoreUInt2(reinterpret_cast<XMUINT2*>(&dst), src);
		return dst;
	}

	CJING_FORCE_INLINE const U32x3 XM_CALLCONV StoreU32x3(VECTOR src) noexcept {
		U32x3 dst;
		XMStoreUInt3(reinterpret_cast<XMUINT3*>(&dst), src);
		return dst;
	}
	
	CJING_FORCE_INLINE const U32x4 XM_CALLCONV StoreU32x4(VECTOR src) noexcept {
		U32x4 dst;
		XMStoreUInt4(reinterpret_cast<XMUINT4*>(&dst), src);
		return dst;
	}

	CJING_FORCE_INLINE const FMat3x3 XM_CALLCONV StoreFMat3x3(MATRIX src) noexcept {
		FMat3x3 dst;
		XMStoreFloat3x3(reinterpret_cast<XMFLOAT3X3*>(&dst), src);
		return dst;
	}

	CJING_FORCE_INLINE const FMat4x4 XM_CALLCONV StoreFMat4x4(MATRIX src) noexcept {
		FMat4x4 dst;
		XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&dst), src);
		return dst;
	}

	////////////////////////////////////////////////////////////////////////////
	// Functions
	////////////////////////////////////////////////////////////////////////////
}

#endif