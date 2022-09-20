#include "level.h"
#include "core\engine.h"
#include "core\profiler\profiler.h"
#include "core\serialization\jsonWriter.h"
#include "content\jsonResource.h"

namespace VulkanTest
{
	namespace
	{
		Mutex actionsMutex;
		Mutex scenesMutex;
		Array<Scene*> scenes;

		enum class SceneEventType
		{
			OnSceneSaving = 0,
			OnSceneSaved,
			OnSceneSaveError,
		};
		void FireSceneEvent(SceneEventType ent, Scene* scene, Guid guid)
		{
		}

		bool LoadSceneImpl(ISerializable::DeserializeStream* data)
		{
			PROFILE_BLOCK("Load scene");
			Logger::Info("Loading scene...");
			F64 beginTime = Timer::GetTimeSeconds();


			Logger::Info("Scene loaded! Time %f s", Timer::GetTimeSeconds() - beginTime);
			return true;
		}

		bool LoadSceneImpl(ResPtr<SceneResource>& sceneRes)
		{
			// Wait for scene loaded
			// Scene resource is small
			sceneRes->WaitForLoaded();

			// Loading scene
			return LoadSceneImpl(sceneRes->GetData());
		}

		bool SaveSceneImpl(Scene* scene, rapidjson_flax::StringBuffer& outData)
		{
			PROFILE_BLOCK("Save scene");
			auto sceneID = scene->GetGUID();
			FireSceneEvent(SceneEventType::OnSceneSaving, scene, sceneID);

			VulkanTest::JsonWriter writer(outData);
			writer.StartObject();
			{
				writer.JKEY("ID");
				writer.Guid(sceneID);
				writer.JKEY("Typename");
				writer.String("SceneResource");

				// Data
				writer.JKEY("Data");
				writer.StartObject();
				{
					// Entities
					writer.JKEY("Entities");
					writer.StartArray();
					writer.EndArray();

					// Scene plugins
					writer.JKEY("Scenes");
					writer.StartArray();
					writer.EndArray();
				}
				writer.EndObject();
				
			}
			writer.EndObject();
			return true;
		}
	}

	class LevelServiceImpl : public EngineService
	{
	private:
		JsonResourceFactory<SceneResource> sceneFactory;

	public:
		LevelServiceImpl() :
			EngineService("LevelService", 0)
		{}

		bool Init(Engine& engine) override
		{
			sceneFactory.Initialize(SceneResource::ResType);

			initialized = true;
			return true;
		}

		void Uninit() override
		{
			scenes.resize(0);
			sceneFactory.Uninitialize();
			initialized = false;
		}
	};
	LevelServiceImpl LevelServiceImplInstance;

	Array<Scene*>& Level::GetScenes()
	{
		return scenes;
	}

	Scene* Level::FindScene(const Guid& guid)
	{
		ScopedMutex lock(scenesMutex);
		for (auto scene : scenes)
		{
			if (scene->GetGUID() == guid)
				return scene;
		}
		return nullptr;
	}

	bool Level::LoadScene(const Guid& guid)
	{
		if (!guid.IsValid())
			return false;

		if (FindScene(guid))
		{
			Logger::Info("Scene is already loaded %d", guid.GetHash());
			return true;
		}

		ResPtr<SceneResource> sceneRes = ResourceManager::LoadResource<SceneResource>(guid);
		if (!sceneRes)
		{
			Logger::Error("Failed to load scene %d", guid.GetHash());
			return false;
		}

		if (!LoadSceneImpl(sceneRes))
		{
			Logger::Error("Failed to load scene %d", guid.GetHash());
			return false;
		}

		return true;
	}

	bool Level::LoadScene(const Path& path)
	{
		Logger::Info("Loading scene from file. Path: \'%s\'", path.c_str());
		auto fs = ResourceManager::GetFileSystem();
		if (!fs->FileExists(path.c_str()))
		{
			Logger::Error("Missing scene file %s", path.c_str());
			return false;
		}

		OutputMemoryStream mem;
		if (!fs->LoadContext(path.c_str(), mem))
		{
			Logger::Error("Failed to load scene file %s", path.c_str());
			return false;
		}

		// Parse scene json
		ISerializable::SerializeDocument document;
		{
			PROFILE_BLOCK("Parse json");
			document.Parse((const char*)mem.Data(), mem.Size());
		}
		if (document.HasParseError())
		{
			Logger::Error("Failed to parse json");
			return false;
		}

		// Get json resource data
		auto dataMember = document.FindMember("Data");
		if (dataMember == document.MemberEnd())
		{
			Logger::Warning("Missing json resource data.");
			return false;
		}

		return LoadSceneImpl(&dataMember->value);
	}

	bool Level::SaveScene(Scene* scene, rapidjson_flax::StringBuffer& outData)
	{
		ScopedMutex lock(actionsMutex);
		Logger::Info("Saving scene %s", scene->GetName());
		F64 beginTime = Timer::GetTimeSeconds();
		if (!SaveSceneImpl(scene, outData))
		{
			Logger::Warning("Failed to save scene");
			return false;
		}

		Logger::Info("Scene saved! Time %f s", Timer::GetTimeSeconds() - beginTime);
		return true;
	}
}