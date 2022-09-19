#include "level.h"
#include "core\engine.h"

namespace VulkanTest
{
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

		void Uninit() override
		{
			initialized = false;
		}
	};
	LevelServiceImpl LevelServiceImplInstance;
}