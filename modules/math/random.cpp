#include "random.h"

#include <random>

namespace VulkanTest
{
namespace Random
{
    std::random_device randomDevice;
    std::mt19937 engine(randomDevice());

    U32 RandomInt(U32 x)
    {
        return RandomInt(0, x);
    }

    U32 RandomInt(U32 x, U32 y)
    {
        std::uniform_int_distribution<U32> dist{};
        using param_type = std::uniform_int_distribution<U32>::param_type;
        return dist(engine, param_type{ x, y });
    }

    F32 RandomFloat(F32 x)
    {
        return RandomFloat(0.0f, x);
    }

    F32 RandomFloat(F32 x, F32 y)
    {
        std::uniform_real_distribution<F32> dist{};
        using param_type = std::uniform_real_distribution<F32>::param_type;
        return dist(engine, param_type{ x, y });
    }

    F64 RandomDouble(F64 x, F64 y)
    {
        std::uniform_real_distribution<F64> dist{};
        using param_type = std::uniform_real_distribution<F64>::param_type;
        return dist(engine, param_type{ x, y });
    }

    F64 RandomDouble(F64 x)
    {
        return RandomDouble(0.0, x);
    }
}
}