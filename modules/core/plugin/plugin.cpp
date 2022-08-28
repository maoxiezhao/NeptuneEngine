#include "plugin.h"
#include "core\engine.h"
#include "core\profiler\profiler.h"
#include "utils\string.h"

namespace VulkanTest
{
	class PluginManagerImpl final : public PluginManager
	{
	public:
		PluginManagerImpl(Engine& engine_) :
			engine(engine_)
		{}

		~PluginManagerImpl()
		{
			for (auto plugin : plugins) {
				CJING_DELETE(plugin);
			}
		}

		void AddPlugin(IPlugin* plugin) override
		{
			plugins.push_back(plugin);
		}

		IPlugin* LoadPlugin(const char* name) override
		{
			return nullptr;
		}

		IPlugin* GetPlugin(const char* name) override
		{
			for (auto plugin : plugins)
			{
				if (EqualString(plugin->GetName(), name))
					return plugin;
			}
			return nullptr;
		}

		const std::vector<IPlugin*>& GetPlugins()const override
		{
			return plugins;
		}

		void InitPlugins() override
		{
			for (auto plugin : plugins) {
				plugin->Initialize();
			}
		}
		
		void UpdatePlugins(F32 deltaTime) override
		{
			PROFILE_FUNCTION();

			for (auto plugin : plugins) {
				plugin->Update(deltaTime);
			}
		}

		void FixedUpdatePlugins() override
		{
			for (auto plugin : plugins) {
				plugin->FixedUpdate();
			}
		}

	private:
		Engine& engine;
		std::vector<IPlugin*> plugins;
	};

	UniquePtr<PluginManager> PluginManager::Create(Engine& engine)
	{
		return CJING_MAKE_UNIQUE<PluginManagerImpl>(engine);
	}
}