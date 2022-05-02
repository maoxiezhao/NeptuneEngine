#include "inputSystem.h"
#include "core\engine.h"

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
		}

	private:
		Engine& engine;
	};

	UniquePtr<InputSystem> InputSystem::Create(Engine& engine_)
	{
		return CJING_MAKE_UNIQUE<InputSystemImpl>(engine_);
	}
}