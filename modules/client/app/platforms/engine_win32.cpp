#include "core\engine.h"
#include "app\app.h"

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

		void Update(F32 dt) override
		{
		}
	};

	UniquePtr<Engine> CreateEngine(const Engine::InitConfig& config)
	{
		return CJING_MAKE_UNIQUE<EngineImpl>(config);
	}
}