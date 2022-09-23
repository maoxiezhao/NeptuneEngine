#include "level.h"
#include "core\engine.h"
#include "core\profiler\profiler.h"
#include "core\serialization\jsonWriter.h"
#include "core\serialization\jsonUtils.h"
#include "content\jsonResource.h"

namespace VulkanTest
{
	// Scene callbacks
	Level::SceneCallback Level::SceneLoading;
	Level::SceneCallback Level::SceneLoaded;
	Level::SceneCallback Level::SceneUnloading;
	Level::SceneCallback Level::SceneUnloaded;
	Level::SceneCallback Level::SceneLoadError;

	// Scene events
	enum class SceneEventType
	{
		OnSceneSaving = 0,
		OnSceneSaved,
		OnSceneSaveError,
		OnSceneLoading,
		OnSceneLoaded,
		OnSceneUnloaded,
		OnSceneLoadError
	};

	// Scene actions
	struct SceneAction
	{
		virtual ~SceneAction() {}
		virtual bool CanDo() const { return true; }
		virtual bool Do() { return true; }
	};

	namespace
	{
		Mutex actionsMutex;
		Mutex scenesMutex;
		Array<Scene*> scenes;
		Array<SceneAction*> actions;

		void FireSceneEvent(SceneEventType ent, Scene* scene, Guid guid)
		{
			switch (ent)
			{
			case SceneEventType::OnSceneSaving:
				break;
			case SceneEventType::OnSceneSaved:
				break;
			case SceneEventType::OnSceneSaveError:
				break;
			case SceneEventType::OnSceneLoaded:
				Level::SceneLoaded.Invoke(scene, guid);
				break;
			case SceneEventType::OnSceneUnloaded:
				Level::SceneUnloaded.Invoke(scene, guid);
				break;
			default:
				break;
			}
		}

		void ProcessSceneActions()
		{
			ScopedMutex lock(actionsMutex);
			while (!actions.empty())
			{
				auto action = actions.front();
				if (!action->CanDo())
					break;
				actions.pop_front();

				action->Do();
				CJING_SAFE_DELETE(action);
			}
		}

		bool LoadSceneImpl(ISerializable::DeserializeStream* data)
		{
			PROFILE_BLOCK("Load scene");
			Logger::Info("Loading scene...");
			F64 beginTime = Timer::GetTimeSeconds();

			auto it = data->FindMember("Scene");
			if (it == data->MemberEnd())
			{
				Logger::Error("Invalid scene resource");
				return false;
			}
			auto& sceneValue = it->value;
			auto sceneID = JsonUtils::GetGuid(sceneValue, "ID");
			if (!sceneID.IsValid())
			{
				Logger::Error("Invalid scene id");
				return false;
			}

			// Check if scene is already loaded
			if (Level::FindScene(sceneID) != nullptr)
			{
				Logger::Info("Scene %d is already loaded.", sceneID.GetHash());
				return true;
			}

			// Create scene instance
			Scene* scene = CJING_NEW(Scene)(ScriptingObjectParams(sceneID));
			scene->Deserialize(sceneValue);

			FireSceneEvent(SceneEventType::OnSceneLoading, scene, sceneID);

			// Deserialize

			// Init scene
			{
				PROFILE_BLOCK("SceneBegin");
				ScopedMutex lock(scenesMutex);
				scenes.push_back(scene);
				// scene->Start();
			}

			FireSceneEvent(SceneEventType::OnSceneLoaded, scene, sceneID);
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

		void UnloadSceneImpl(Scene* scene)
		{
			if (scene == nullptr)
				return;

			PROFILE_BLOCK("Unload scene");
	
			//if (scene->IsPlaying())
			//	scene->Stop();

			scenes.erase(scene);
			scene->DeleteObject();

			FireSceneEvent(SceneEventType::OnSceneUnloaded, scene, scene->GetGUID());

			ObjectService::FlushNow();
		}

		void UnloadAllScenesImpl()
		{
			for (int i = 0; i < scenes.size(); i++)
				UnloadSceneImpl(scenes[i]);
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
					// Scene data
					writer.JKEY("Scene");
					scene->Serialize(writer, nullptr);

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

	struct LoadSceneAction : public SceneAction
	{
		Guid sceneID;
		ResPtr<SceneResource> sceneRes;

		LoadSceneAction(const Guid& sceneID_, SceneResource* sceneRes_) :
			sceneID(sceneID_),
			sceneRes(sceneRes_)
		{
		}

		bool CanDo()const override
		{
			return sceneRes != nullptr && sceneRes->IsLoaded();
		}

		bool Do() override
		{
			if (!LoadSceneImpl(sceneRes))
			{
				Logger::Error("Failed to deserialize scene %d", sceneID.GetHash());
				FireSceneEvent(SceneEventType::OnSceneLoadError, nullptr, sceneID);
				return false;
			}

			return true;
		}
	};

	struct UnloadSceneAction : public SceneAction
	{
		Guid sceneID;

		UnloadSceneAction(Scene* scene_) :
			sceneID(scene_->GetGUID())
		{
		}

		bool Do() override
		{
			auto scene = Level::FindScene(sceneID);
			if (scene == nullptr)
				return false;

			UnloadSceneImpl(scene);
			return true;
		}
	};

	class LevelServiceImpl : public EngineService
	{
	public:
		LevelServiceImpl() :
			EngineService("LevelService", 0)
		{}

		bool Init(Engine& engine) override
		{
			initialized = true;
			return true;
		}

		void LateUpdate()override
		{
			PROFILE_FUNCTION();
			ProcessSceneActions();
		}

		void Uninit() override
		{
			UnloadAllScenesImpl();
			scenes.resize(0);
			actions.resize(0);
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

	bool Level::LoadSceneAsync(const Guid& guid)
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

		ScopedMutex lcok(actionsMutex);
		actions.push_back(CJING_NEW(LoadSceneAction)(guid, sceneRes));
		return true;
	}

	bool Level::UnloadScene(Scene* scene)
	{
		UnloadSceneImpl(scene);
		return true;
	}

	bool Level::UnloadSceneAsync(Scene* scene)
	{
		ScopedMutex lcok(actionsMutex);
		actions.push_back(CJING_NEW(UnloadSceneAction)(scene));
		return true;
	}

	void Level::UnloadAllScenes()
	{
		ScopedMutex lock(scenesMutex);
		UnloadAllScenesImpl();
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