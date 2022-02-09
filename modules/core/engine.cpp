#include "engine.h"

namespace VulkanTest
{
	class EngineImpl final : public Engine
	{
	public:
		EngineImpl(const InitConfig& initConfig)
		{
		}

		virtual ~EngineImpl()
		{
		}
	};

	UniquePtr<Engine> Engine::Create(const InitConfig& config)
	{
		return CJING_MAKE_UNIQUE<EngineImpl>(config);
	}
}