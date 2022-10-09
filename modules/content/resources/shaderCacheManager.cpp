#include "shaderCacheManager.h"
#include "core\engine.h"
#include "core\globals.h"

namespace VulkanTest
{
#define SHADER_CACHE_VERSION 1

	struct ShaderDatabase
	{
		Path folder;

		void Init(const Path& cacheRoot)
		{
			folder = cacheRoot;
		}

		bool TryGetEntry(const Guid& id, ShaderCacheManager::CachedEntry& cachedEntry)
		{
			ASSERT(id.IsValid());
			cachedEntry.ID = id;
			cachedEntry.Path = folder / id.ToString(Guid::FormatType::P);
			return cachedEntry.Exists();
		}

		bool GetCache(const ShaderCacheManager::CachedEntry& cachedEntry, OutputMemoryStream& output)
		{
			return FileSystem::LoadContext(cachedEntry.Path, output);
		}

		bool SaveCache(const ShaderCacheManager::CachedEntry& cachedEntry, OutputMemoryStream& input)
		{
			auto file = FileSystem::OpenFile(cachedEntry.Path, FileFlags::DEFAULT_WRITE);
			if (!file)
				return false;
			file->Write(input.Data(), input.Size());
			file->Close();
			return true;
		}
	};
	ShaderDatabase shaderDatabase;

	class ShaderCacheManagerService : public EngineService
	{
	public:
		ShaderCacheManagerService()
			: EngineService(("ShaderCacheManagerService"), -200)
		{
		}

		bool Init(Engine& engine) override
		{
#ifdef CJING3D_EDITOR
			const Path rootDir = Globals::ProjectCacheFolder / "shaders";
#else
			const Path rootDir = Globals::ProjectLocalFolder / "shaders";
#endif
			U32 version = 0;
			const Path cacheVerFile = rootDir / "cacheVersion.bin";
			if (FileSystem::FileExists(cacheVerFile))
			{
				OutputMemoryStream mem;
				if (!FileSystem::LoadContext(cacheVerFile.c_str(), mem))
				{
					Logger::Warning("Failed to read the shaders cache version file");
					version = 0;
				}
				InputMemoryStream input(mem);
				input.Read(version);
			}

			if (version != SHADER_CACHE_VERSION)
			{
				Logger::Warning("Invalid shaders cache database.");
				if (Platform::DirExists(rootDir))
					Platform::DeleteFile(rootDir);

				if (!Platform::MakeDir(rootDir))
				{
					Logger::Warning("Failed to create the shader cache directory");
				}

				version = SHADER_CACHE_VERSION;
				auto file = FileSystem::OpenFile(cacheVerFile, FileFlags::DEFAULT_WRITE);
				if (file)
				{
					file->Write(version);
					file->Close();
				}
			}

			shaderDatabase.Init(rootDir);
			return true;
		}
	};
	ShaderCacheManagerService ShaderCacheManagerServiceInstance;

	bool ShaderCacheManager::CachedEntry::IsValid() const
	{
		return ID.IsValid();
	}

	bool ShaderCacheManager::CachedEntry::Exists() const
	{
		return FileSystem::FileExists(Path);
	}

	U64 ShaderCacheManager::CachedEntry::GetLastModTime() const
	{
		return FileSystem::GetLastModTime(Path);
	}

	bool ShaderCacheManager::TryGetEntry(const Guid& id, CachedEntry& cachedEntry)
	{
		return shaderDatabase.TryGetEntry(id, cachedEntry);
	}

	bool ShaderCacheManager::GetCache(const CachedEntry& cachedEntry, OutputMemoryStream& output)
	{
		return shaderDatabase.GetCache(cachedEntry, output);
	}

	bool ShaderCacheManager::SaveCache(const CachedEntry& cachedEntry, OutputMemoryStream& input)
	{
		return shaderDatabase.SaveCache(cachedEntry, input);
	}
}