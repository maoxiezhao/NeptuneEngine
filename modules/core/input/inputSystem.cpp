#include "inputSystem.h"
#include "core\engine.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
	class InputSystemImpl : public InputSystem
	{
	public:
		InputSystemImpl(Engine& engine_) :
			engine(engine_)
		{}

		~InputSystemImpl()
		{
		}

		void Update(F32 dt) override
		{
			PROFILE_FUNCTION();
		}

	private:
		Engine& engine;
	};

	UniquePtr<InputSystem> InputSystem::Create(Engine& engine_)
	{
		return CJING_MAKE_UNIQUE<InputSystemImpl>(engine_);
	}
}