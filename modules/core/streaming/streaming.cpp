#include "streaming.h"
#include "core\engine.h"
#include "core\jobsystem\taskGraph.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
	class StreamingSystem final : public TaskGraphSystem
	{
	public:
		void Execute(Jobsystem::JobHandle* handle) override
		{
			Jobsystem::Run(nullptr, [&](void* data) {
				PROFILE_BLOCK("Streaming::Execute");
				

			}, handle);
		}
	};

	class StreamingServiceImpl : public EngineService
	{
	public:
		StreamingServiceImpl() :
			EngineService("StreamingService", 0)
		{}

		bool Init(Engine& engine) override
		{
			system = CJING_NEW(StreamingSystem);
			engine.GetTaskGraph().AddSystem(system);
			initialized = true;
			return true;
		}

		void Uninit() override
		{
			CJING_SAFE_DELETE(system);
			initialized = false;
		}

	public:
		StreamingSystem* system = nullptr;
		Mutex mutex;
	};
	StreamingServiceImpl StreamingServiceImplInstance;

	void Streaming::RequestStreamingUpdate()
	{
		PROFILE_FUNCTION();
		ScopedMutex lock(StreamingServiceImplInstance.mutex);

	}
}