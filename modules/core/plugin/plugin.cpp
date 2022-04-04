#include "plugin.h"
#include "core\engine.h"

namespace VulkanTest
{
	class PluginManagerImpl final : public PluginManager
	{
	public:
		PluginManagerImpl(Engine& engine_) :
			engine(engine_)
		{}

		const std::vector<IPlugin*>& GetPlugins()const override
		{
			return plugins;
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