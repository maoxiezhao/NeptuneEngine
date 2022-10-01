#include "changingScenesState.h"
#include "editorStateMachine.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	ChangingScenesState::ChangingScenesState(EditorStateMachine& machine, EditorApp& editor_) :
		EditorState(machine, editor_)
	{
	}

	ChangingScenesState::~ChangingScenesState()
	{
		toUnloadScenes.resize(0);
		toLoadScenes.resize(0);
		EditorState::~EditorState();
	}

	void ChangingScenesState::OnSceneEventCallback(Scene* scene, const Guid& sceneID)
	{
		if (sceneID == lastRequestScene)
		{
			lastRequestScene = Guid::Empty;
			stateMachine.SwitchState(EditorStateType::EditingScene);
		}
	}

	void ChangingScenesState::OnEnter()
	{
		ASSERT(lastRequestScene == Guid::Empty);

		Level::SceneLoaded.Bind<&ChangingScenesState::OnSceneEventCallback>(this);
		Level::SceneUnloaded.Bind<&ChangingScenesState::OnSceneEventCallback>(this);

		for (auto scene : toUnloadScenes)
			Level::UnloadSceneAsync(scene);
		toUnloadScenes.clear();

		for (const auto& sceneID : toLoadScenes)
		{
			if (Level::LoadSceneAsync(sceneID))
				lastRequestScene = sceneID;
		}
		toLoadScenes.clear();

		if (lastRequestScene == Guid::Empty)
		{
			Logger::Warning("Failed to perform scene");
			stateMachine.SwitchState(EditorStateType::EditingScene);
		}
	}

	void ChangingScenesState::OnExit()
	{
		Level::SceneLoaded.Unbind<&ChangingScenesState::OnSceneEventCallback>(this);
		Level::SceneUnloaded.Unbind<&ChangingScenesState::OnSceneEventCallback>(this);

		lastRequestScene = Guid::Empty;
	}

	const char* ChangingScenesState::GetName() const
	{
		return "ChangingScenes";
	}

	void ChangingScenesState::LoadScene(const Guid& guid)
	{
		toLoadScenes.clear();
		toUnloadScenes.clear();
		toLoadScenes.push_back(guid);
		TryEnter();
	}

	void ChangingScenesState::UnloadScene(Scene* scene)
	{
		ASSERT(scene != nullptr);
		toLoadScenes.clear();
		toUnloadScenes.clear();
		toUnloadScenes.push_back(scene);
		TryEnter();
	}

	void ChangingScenesState::TryEnter()
	{
		// Check unload scenes
		for (int i = 0; i < (int)toUnloadScenes.size(); i++)
		{
			Guid sceneID = toUnloadScenes[i]->GetGUID();
			for (int j = 0; j < (int)toLoadScenes.size(); i++)
			{
				if (toLoadScenes[i] == sceneID)
				{
					toLoadScenes.eraseAt(j);
					toUnloadScenes.eraseAt(i);
					break;
				}
			}
		}

		// Skip loaded scenes
		for (int i = 0; i < (int)toLoadScenes.size(); i++)
		{
			if (Level::FindScene(toLoadScenes[i]) != nullptr)
			{
				toLoadScenes.swapAndPopItem(toLoadScenes[i]);
				if (toLoadScenes.size() == 0)
					break;
				i--;
			}
		}

		parent->GoToState(this);
	}
}
}