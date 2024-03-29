#pragma once

#include "core\common.h"
#include "scene.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Level
	{
	public:
		// Scene event callbacks
		using SceneCallback = DelegateList<void(Scene*, const Guid&)>;
		static SceneCallback SceneLoading;
		static SceneCallback SceneLoaded;
		static SceneCallback SceneUnloading;
		static SceneCallback SceneUnloaded;
		static SceneCallback SceneLoadError;
		
		// Serialize callback
		using SceneSerializingCallback = DelegateList<void(ISerializable::SerializeStream& data, Scene* scene)>;
		static SceneSerializingCallback SceneSerializing;

		// Deserialize callback
		using SceneDeserializingCallback = DelegateList<void(ISerializable::DeserializeStream* data, Scene* scene)>;
		static SceneDeserializingCallback sceneDeserializing;

	public:
		static Array<Scene*>& GetScenes();
		static Scene* FindScene(const Guid& guid);
		static bool LoadScene(const Guid& guid);
		static bool LoadScene(const Path& path);
		static bool LoadSceneAsync(const Guid& guid);
		static bool UnloadScene(Scene* scene);
		static bool UnloadSceneAsync(Scene* scene);
		static void UnloadAllScenes();
		static bool SaveScene(Scene* scene);
		static bool SaveScene(Scene* scene, rapidjson_flax::StringBuffer& outData);
		static void SaveSceneAsync(Scene* scene);
	};
}