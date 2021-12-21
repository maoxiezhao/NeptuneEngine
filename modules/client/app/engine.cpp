#include "engine.h"
#include "app.h"

namespace VulkanTest
{
	class EngineImpl final : public Engine
	{
	public:
		EngineImpl(const App& app)
		{
		}

		virtual ~EngineImpl()
		{
		}
	};

	UniquePtr<Engine> Engine::Create(App& app)
	{
		return CJING_MAKE_UNIQUE<EngineImpl>(app);
	}
}