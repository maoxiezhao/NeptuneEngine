#include "math.hpp"
#include "vMath_impl.hpp"

namespace VulkanTest
{
	void Transform::UpdateTransform()
	{
		if (isDirty)
		{
			isDirty = false;
			world = StoreFMat4x4(GetMatrix());
		}
	}

	MATRIX Transform::GetMatrix() const
	{
		VECTOR s = LoadF32x3(scale);
		VECTOR r = LoadF32x4(rotation);
		VECTOR t = LoadF32x3(translation);
		return
			MatrixScalingFromVector(s) *
			MatrixRotationQuaternion(r) *
			MatrixTranslationFromVector(t);
	}

	void Transform::Translate(const F32x3& value)
	{
		isDirty = true;
		translation += value;
	}

	void Transform::RotateRollPitchYaw(const F32x3& value)
	{
		isDirty = true;

		VECTOR quat = LoadF32x4(rotation);
		VECTOR x = QuaternionRotationRollPitchYaw(value.x, 0, 0);
		VECTOR y = QuaternionRotationRollPitchYaw(0, value.y, 0);
		VECTOR z = QuaternionRotationRollPitchYaw(0, 0, value.z);

		quat = QuaternionMultiply(x, quat);
		quat = QuaternionMultiply(quat, y);
		quat = QuaternionMultiply(z, quat);
		quat = QuaternionNormalize(quat);

		rotation = StoreF32x4(quat);
	}

	void Transform::Rotate(const F32x4& quaternion)
	{
		isDirty = true;

		VECTOR quat = QuaternionMultiply(LoadF32x4(rotation), LoadF32x4(quaternion));
		quat = QuaternionNormalize(quat);
		rotation = StoreF32x4(quat);
	}

	void Transform::Scale(const F32x3& value)
	{
		isDirty = true;
		scale *= value;
	}

	F32x3 Transform::DoTransform(const F32x3& value) const
	{
		return StoreF32x3(Vector4Transform(LoadF32x3(value), LoadFMat4x4(world)));
	}

	F32x4 Transform::DoTransform(const F32x4& value) const
	{
		return StoreF32x4(Vector3Transform(LoadF32x4(value), LoadFMat4x4(world)));
	}
}