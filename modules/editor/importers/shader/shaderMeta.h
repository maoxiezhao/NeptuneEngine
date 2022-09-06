#pragma once

#include "renderer\shader.h"

namespace VulkanTest
{
namespace Editor
{
    struct ShaderPermutationEntry
    {
        String name;
        I32 value;
    };

    struct ShaderPermutation
    {
        // Array<ShaderPermutationEntry> entries;
        std::vector<ShaderPermutationEntry> entries;
    };

    struct ShaderFunctionMeta
    {
    public:
        String name;
        // Array<ShaderPermutation> permutations;
        std::vector<ShaderPermutation> permutations;
        bool visible = true;

    public:
        virtual ~ShaderFunctionMeta() = default;
        virtual GPU::ShaderStage GetStage() const = 0;

        bool HasDefinition(I32 permutationIndex, const String& defineName) const
        {
            ASSERT(permutationIndex < (I32)permutations.size());
            auto& permutation = permutations[permutationIndex];
            for (I32 i = 0; i < (I32)permutation.entries.size(); i++)
            {
                if (permutation.entries[i].name == defineName)
                    return true;
            }

            return false;
        }

        bool HasDefinition(const String& defineName) const
        {
            for (I32 permutationIndex = 0; permutationIndex < (I32)permutations.size(); permutationIndex++)
            {
                if (HasDefinition(permutationIndex, defineName))
                    return true;
            }
            return false;
        }

        void GetDefinitionsForPermutation(I32 permutationIndex, std::vector<GPU::ShaderMacro>& macros) const
        {
            ASSERT(permutationIndex < (I32)permutations.size());
            auto& permutation = permutations[permutationIndex];
            for (I32 i = 0; i < (I32)permutation.entries.size(); i++)
            {
                auto& e = permutation.entries[i];
                macros.push_back({ e.name.c_str(), e.value});
            }
        }
    };

    struct VertexShaderMeta : ShaderFunctionMeta
    {
        GPU::ShaderStage GetStage() const override { return GPU::ShaderStage::VS; }
    };

    struct PixelShaderMeta : ShaderFunctionMeta
    {
        GPU::ShaderStage GetStage() const override { return GPU::ShaderStage::PS; }
    };

    struct ComputeShaderMeta : ShaderFunctionMeta
    {
        GPU::ShaderStage GetStage() const override { return GPU::ShaderStage::CS; }
    };

    struct ShaderMeta
    {
        Array<VertexShaderMeta> vs;
        Array<PixelShaderMeta> ps;
        Array<ComputeShaderMeta> cs;

        U32 GetShadersCount() const
        {
            return vs.size() + ps.size() + cs.size();
        }
    };
}
}
