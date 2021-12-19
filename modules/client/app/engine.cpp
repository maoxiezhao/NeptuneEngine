#include "engine.h"

namespace VulkanTest
{
	class EngineImpl final : public Engine
	{
	public:
		EngineImpl(const EngineInitConfig& initConfig)
		{
		}

		virtual ~EngineImpl()
		{
		}
	};

	UniquePtr<Engine> Engine::Create(const EngineInitConfig& initConfig)
	{
		return CJING_MAKE_UNIQUE<EngineImpl>(initConfig);
	}
}