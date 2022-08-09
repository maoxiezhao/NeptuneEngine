#include "shader.h"
#include "renderer.h"
#include "core\scripts\luaConfig.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Shader);

	const U32 Shader::FILE_MAGIC = 0x5f3cfd1f;
	const U32 Shader::FILE_VERSION = 0x01;

	void Shader::ShaderContainer::Clear()
	{
		//GPU::DeviceVulkan* device = Renderer::GetDevice();
		//if (device != nullptr)
		//{
		//	for (auto shader : shaders)
		//		device->ReleaseShader(shader);
		//}
		shaders.clear();
	}

	void Shader::ShaderContainer::Add(GPU::Shader* shader, const String& name, I32 permutationIndex)
	{
		shaders.insert(CalculateHash(name, permutationIndex), shader);
	}

	GPU::Shader* Shader::ShaderContainer::Get(const String& name, I32 permutationIndex)
	{
		auto it = shaders.find(CalculateHash(name, permutationIndex));
		return it.isValid() ? it.value() : nullptr;
	}

	U64 Shader::ShaderContainer::CalculateHash(const String& name, I32 permutationIndex)
	{
		HashCombiner hash;
		hash.HashCombine(RuntimeHash(name.c_str()).GetHashValue());
		hash.HashCombine(permutationIndex);
		return hash.Get();
	}

	Shader::Shader(const Path& path_, ResourceFactory& resFactory_) :
		BinaryResource(path_, resFactory_)
	{
	}

	Shader::~Shader()
	{
		ASSERT(IsEmpty());
	}

	bool Shader::Create(U64 size, const U8* mem)
	{
		InputMemoryStream inputMem(mem, size);
		bool isReady = LoadFromMemory(inputMem);
		OnCreated(isReady ? Resource::State::READY : Resource::State::FAILURE);
		return isReady;
	}

	GPU::Shader* Shader::GetShader(GPU::ShaderStage stage, const String& name, I32 permutationIndex)
	{
		auto shader = shaders.Get(name, permutationIndex);
		if (shader == nullptr)
			Logger::Error("Missing shader %s %d", name.c_str(), permutationIndex);
		else if (shader->GetStage() != stage)
			Logger::Error("Invalid shader stage %s %d, Expected : %d, get %d", name.c_str(), permutationIndex, (U32)stage, (U32)shader->GetStage());
		
		return shader;
	}

	bool Shader::OnLoaded()
	{
		PROFILE_FUNCTION();
		const auto dataChunk = GetChunk(0);
		if (dataChunk == nullptr || !dataChunk->IsLoaded())
			return false;

		InputMemoryStream inputMem(dataChunk->Data(), dataChunk->Size());
		FileHeader header;
		inputMem.Read<FileHeader>(header);
		if (header.magic != FILE_MAGIC)
		{
			Logger::Warning("Unsupported shader file %s", GetPath());
			return false;
		}

		if (header.version != FILE_VERSION)
		{
			Logger::Warning("Unsupported version of shader %s", GetPath());
			return false;
		}

		return LoadFromMemory(inputMem);
	}

	void Shader::OnUnLoaded()
	{
		shaders.Clear();
	}

	bool Shader::LoadFromMemory(InputMemoryStream& inputMem)
	{
		GPU::DeviceVulkan* device = Renderer::GetDevice();
		ASSERT(device != nullptr);

		// Shader format:
		// -------------------------
		// Shader count
		// -- Stage
		// -- Permutations count
		// -- Function name
		// ----- CachesSize
		// ----- Cache
		// ----- ResLayout

		U32 shaderCount = 0;
		inputMem.Read(shaderCount);
		if (shaderCount <= 0)
			return false;

		for (U32 i = 0; i < shaderCount; i++)
		{
			// Stage
			U8 stageValue = 0;
			inputMem.Read(stageValue);

			// Permutations count
			U32 permutationsCount = 0;
			inputMem.Read(permutationsCount);

			// Name
			U32 strSize;
			inputMem.Read(strSize);
			char name[MAX_PATH_LENGTH];
			name[strSize] = 0;
			inputMem.Read(name, strSize);

			if (permutationsCount <= 0)
				continue;

			for (U32 permutationIndex = 0; permutationIndex < permutationsCount; permutationIndex++)
			{
				GPU::ShaderResourceLayout resLayout;
				I32 cacheSize = 0;
				inputMem.Read(cacheSize);
				if (cacheSize <= 0)
				{
					inputMem.Read(resLayout);
					continue;
				}

				// Cache
				OutputMemoryStream cache;
				cache.Resize(cacheSize);
				inputMem.Read(cache.Data(), cacheSize);

				// ShaderResourceLayout
				inputMem.Read(resLayout);

				GPU::Shader* shader = device->RequestShader(GPU::ShaderStage(stageValue), cache.Data(), cache.Size(), &resLayout);
				if (shader == nullptr)
				{
					Logger::Warning("Failed to create shader");
					return false;
				}
				shader->SetEntryPoint(name);
				shaders.Add(shader, name, permutationIndex);
			}
		}
		return true;
	}
}