#include "shaderBase.h"
#include "shaderCacheManager.h"
#include "core\scripts\luaConfig.h"
#include "core\profiler\profiler.h"

#if COMPILE_WITH_SHADER_COMPILER
#include "shadersCompilation\shaderCompilation.h"
#endif

namespace VulkanTest
{
	const U32 ShaderStorage::Header::FILE_MAGIC = 0x5f3cfd1f;
	const U32 ShaderStorage::Header::FILE_VERSION = 0x01;

	ShaderStorage::CachingMode ShaderStorage::CurrentCachingMode =
#if CJING3D_EDITOR
		CachingMode::ProjectCache;
#else
		CachingMode::ResourceInternal;
#endif

	bool ShaderContainer::LoadFromMemory(InputMemoryStream& inputMem)
	{
		GPU::DeviceVulkan* device = GPU::GPUDevice::Instance;
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
				Add(shader, name, permutationIndex);
			}
		}
		return true;
	}

	void ShaderContainer::Clear()
	{
		shaders.clear();
	}

	void ShaderContainer::Add(GPU::Shader* shader, const String& name, I32 permutationIndex)
	{
		shaders.insert(CalculateHash(name, permutationIndex), shader);
	}

	GPU::Shader* ShaderContainer::Get(const String& name, I32 permutationIndex)
	{
		auto it = shaders.find(CalculateHash(name, permutationIndex));
		return it.isValid() ? it.value() : nullptr;
	}

	U64 ShaderContainer::CalculateHash(const String& name, I32 permutationIndex)
	{
		HashCombiner hash;
		hash.HashCombine(RuntimeHash(name.c_str()).GetHashValue());
		hash.HashCombine(permutationIndex);
		return hash.Get();
	}

#if COMPILE_WITH_SHADER_COMPILER
	bool IsShaderCacheValid(OutputMemoryStream& data)
	{
		return true;
	}
#endif

#if CJING3D_EDITOR
	bool ShaderResourceBase::Save()
	{
		return true;
	}

#endif

	bool ShaderResourceBase::LoadShaderCache(ShaderCacheResult& result)
	{
		auto shaderRes = GetShaderResource();
		if (shaderRes == nullptr)
			return false;

		const I32 cacheChunkIndex = GetCacheChunkIndex();
#if COMPILE_WITH_SHADER_COMPILER
		ShaderCacheManager::CachedEntry shaderCacheEntry;
		auto cachingMode = ShaderStorage::CurrentCachingMode;
		bool hasCache = false;
		if (cachingMode == ShaderStorage::CachingMode::ResourceInternal)
		{
			// Load shader cache from the shader resource
			if (shaderRes->HasChunkLoaded(cacheChunkIndex))
			{
				auto chunk = shaderRes->GetChunk(cacheChunkIndex);
				result.Data.Link(chunk->Data(), chunk->Size());
				if (IsShaderCacheValid(result.Data))
				{
					hasCache = true;
				}
				else
				{
					result.Data.Clear();
				}
			}
		}
		else if (cachingMode == ShaderStorage::CachingMode::ProjectCache)
		{
			// Load shader cache from the cache file
			if (ShaderCacheManager::TryGetEntry(shaderRes->GetGUID(), shaderCacheEntry))
			{
				U64 time = shaderRes->storage ? FileSystem::GetLastModTime(shaderRes->storage->GetPath()) : 0;
				if (shaderCacheEntry.GetLastModTime() > time &&
					ShaderCacheManager::GetCache(shaderCacheEntry, result.Data) &&
					IsShaderCacheValid(result.Data))
				{
					hasCache = true;
				}
				else
				{
					result.Data.Clear();
				}
			}
		}

		if (hasCache == false && shaderRes->HasChunk(SHADER_RESOURCE_CHUNK_SOURCE))
		{
			result.Data.Clear();

			Logger::Info("Compiling shader %s...", shaderRes->GetPath().c_str());
			if (!shaderRes->LoadChunks(GET_CHUNK_FLAG(SHADER_RESOURCE_CHUNK_SOURCE)))
			{
				Logger::Warning("Failed to load shader source chunk data %s", shaderRes->GetPath().c_str());
				return false;
			}
			shaderRes->ReleaseChunk(cacheChunkIndex);

			// Load shader source data
			auto sourceChunk = shaderRes->GetChunk(SHADER_RESOURCE_CHUNK_SOURCE);
			char* sourceData = (char*)sourceChunk->Data();
			const size_t sourceLength = sourceChunk->Size();
			sourceData[sourceLength - 1] = 0;

			// Compile shader
			OutputMemoryStream outMem;
			outMem.Reserve(32 * 1024);
			CompilationOptions options = {};
			options.Macros.clear();
			options.targetName = Path::GetBaseName(shaderRes->GetPath().c_str());
			options.targetGuid = shaderRes->GetGUID();
			options.source = sourceData;
			options.sourceLength = sourceLength;
			options.outMem = &outMem;
			if (!ShaderCompilation::Compile(options))
			{
				Logger::Error("Failed to compile shader %s", shaderRes->GetPath().c_str());
				return false;
			}
			Logger::Info("shader %s compiled.", shaderRes->GetPath().c_str());

			// Save shader cache
			if (cachingMode == ShaderStorage::CachingMode::ResourceInternal)
			{
				// Save cache to the internal shader cache chunk
				auto chunk = shaderRes->GetOrCreateChunk(cacheChunkIndex);
				if (chunk == nullptr)
				{
					Logger::Error("Failed to allocate chunk data %s", shaderRes->GetPath().c_str());
					return false;
				}
				chunk->mem.Write(outMem.Data(), outMem.Size());

#if CJING3D_EDITOR
				// Save shader resource
				if (Save())
				{
					Logger::Warning("Cannot save '%s'.", shaderRes->GetPath().c_str());
					return false;
				}
#endif
			}
			else if (cachingMode == ShaderStorage::CachingMode::ProjectCache)
			{
				// Save results to cache
				if (!ShaderCacheManager::SaveCache(shaderCacheEntry, outMem))
				{
					Logger::Warning("Cannot save shader cache.");
					return false;
				}
			}
		}
#endif
		
		if (shaderRes->HasChunkLoaded(cacheChunkIndex))
		{
			auto chunk = shaderRes->GetChunk(cacheChunkIndex);
			result.Data.Link(chunk->Data(), chunk->Size());

		}
#if COMPILE_WITH_SHADER_COMPILER
		else if (shaderCacheEntry.IsValid())
		{
			if (!ShaderCacheManager::GetCache(shaderCacheEntry, result.Data))
			{
				Logger::Warning("Missing shader cache %s", shaderRes->GetPath().c_str());
				return false;
			}
		}
#endif
		else
		{
			Logger::Warning("Missing shader cache %s", shaderRes->GetPath().c_str());
			return false;
		}

		return true;
	}

	I32 ShaderResourceBase::GetCacheChunkIndex()
	{
		return SHADER_RESOURCE_CHUNK_SHADER_CACHE;
	}

	bool ShaderResourceBase::InitBase(ResourceInitData& initData)
	{
		InputMemoryStream inputMem(initData.customData);
		inputMem.Read<ShaderStorage::Header>(shaderHeader);
		if (shaderHeader.magic != ShaderStorage::Header::FILE_MAGIC)
		{
			Logger::Warning("Unsupported shader file");
			return false;
		}

		if (shaderHeader.version != ShaderStorage::Header::FILE_VERSION)
		{
			Logger::Warning("Unsupported version of shader");
			return false;
		}
		return true;
	}
}