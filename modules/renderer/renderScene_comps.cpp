#include "renderScene_comps.h"
#include "RenderScene.h"
#include "renderer.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
    void CameraComponent::UpdateCamera()
    {
        projection = StoreFMat4x4(MatrixPerspectiveFovLH(fov, width / height, farZ, nearZ)); // reverse zbuffer!

        VECTOR _Eye = LoadF32x3(eye);
        VECTOR _At  = LoadF32x3(at);
        VECTOR _Up  = LoadF32x3(up);

        MATRIX _V = MatrixLookToLH(_Eye, _At, _Up);
        MATRIX _P = LoadFMat4x4(projection);
        MATRIX _VP = MatrixMultiply(_V, _P);

        view = StoreFMat4x4(_V);
        viewProjection = StoreFMat4x4(_VP);
        invProjection = StoreFMat4x4(MatrixInverse(_P));
        invViewProjection = StoreFMat4x4(MatrixInverse(_VP));

        frustum.Compute(LoadFMat4x4(viewProjection));
    }

    void CameraComponent::UpdateTransform(const Transform& transform)
    {
        MATRIX mat = LoadFMat4x4(transform.world);

        VECTOR Eye = Vector3Transform(VectorSet(0, 0, 0, 1), mat);
        VECTOR At = Vector3Normalize(Vector3TransformNormal(XMVectorSet(0, 0, 1, 0), mat));
        VECTOR Up = Vector3Normalize(Vector3TransformNormal(XMVectorSet(0, 1, 0, 0), mat));

        MATRIX _V = MatrixLookToLH(Eye, At, Up);
        view = StoreFMat4x4(_V);
        rotationMat = StoreFMat3x3(MatrixInverse(_V));

        eye = StoreF32x3(Eye);
        at = StoreF32x3(At);
        up = StoreF32x3(Up);
    }
}