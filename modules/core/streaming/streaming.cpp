#include "streaming.h"
#include "streamingHandler.h"
#include "core\engine.h"
#include "core\jobsystem\taskGraph.h"
#include "core\profiler\profiler.h"
#include "core\platform\timer.h"

namespace VulkanTest
{
	const U32 MaxResourcesPerUpdate = 64;

	class StreamingSystem final : public TaskGraphSystem
	{
	public:
		void Execute(Jobsystem::JobHandle* handle) override;
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
		Array<StreamableResource*> resources;
		I32 lastUpdateIndex = 0;
	};
	StreamingServiceImpl StreamingServiceImplInstance;


	StreamableResource::StreamableResource(IStreamingHandler* streamingHandler_) :
		streamingHandler(streamingHandler_)
	{
		ASSERT(streamingHandler != nullptr);
	}

	StreamableResource::~StreamableResource()
	{
		StopStreaming();
	}

	void StreamableResource::StartStreaming(bool isDynamic_)
	{
		isDynamic = isDynamic_;
		// Is Already streaming
		if (isStreaming)
			return;

		isStreaming = true;
		ScopedMutex lock(StreamingServiceImplInstance.mutex);
		StreamingServiceImplInstance.resources.push_back(this);
	}

	void StreamableResource::StopStreaming()
	{
		if (!isStreaming)
			return;

		isStreaming = false;
		LastUpdateTime = 0.0f;
		ScopedMutex lock(StreamingServiceImplInstance.mutex);
		StreamingServiceImplInstance.resources.erase(this);
	}

	void StreamableResource::CancleStreaming()
	{
		TargetResidency = 0;
		LastUpdateTime = FLT_MAX;
	}

	void StreamableResource::RequestStreamingUpdate()
	{
		LastUpdateTime = 0.0f;
	}

	void UpdataStreamableResource(StreamableResource* res, U64 time)
	{
		ASSERT(res != nullptr);
		auto streamingHandler = res->GetStreamingHandler();
		ASSERT(streamingHandler != nullptr);

		res->LastUpdateTime = time;

		// Calculate target residency of current resource
		I32 maxResidency = res->GetMaxResidency();
		I32 curResidency = res->GetCurrentResidency();
		I32 targetResidency = streamingHandler->CalculateResidency(res);
		ASSERT(targetResidency >= 0 && targetResidency <= maxResidency);

		res->TargetResidency = targetResidency;

		// If target residency is changed and is not allocated
		if (curResidency != targetResidency)
		{
			I32 requested = streamingHandler->CalculateRequestedResidency(res, targetResidency);
			Task* task = res->CreateStreamingTask(requested);
			if (task)
				task->Start();
		}
	}

	void StreamingSystem::Execute(Jobsystem::JobHandle* handle)
	{
		Jobsystem::Run(nullptr, [&](void* data) {
			PROFILE_BLOCK("Streaming::Execute");
			ScopedMutex lock(StreamingServiceImplInstance.mutex);

			auto& lastUpdateIndex = StreamingServiceImplInstance.lastUpdateIndex;
			auto& resources = StreamingServiceImplInstance.resources;
			const U32 resourcesCount = resources.size();
			I32 resourcesUpdates = std::min(MaxResourcesPerUpdate, resourcesCount);

			const auto now = Timer::GetRawTimestamp();
			I32 checkCount = resourcesCount;
			while (resourcesUpdates > 0 && checkCount-- > 0)
			{
				lastUpdateIndex++;
				if (lastUpdateIndex >= resourcesCount)
					lastUpdateIndex = 0;

				auto res = resources[lastUpdateIndex];

				const static U64 ResourceUpdateInterval = (U64)(0.1f * Timer::GetFrequency());
				if (now - res->LastUpdateTime >= ResourceUpdateInterval)
				{
					UpdataStreamableResource(res, now);
					resourcesUpdates--;
				}
			}
		}, handle);
	}

	void Streaming::RequestStreamingUpdate()
	{
		PROFILE_FUNCTION();
		ScopedMutex lock(StreamingServiceImplInstance.mutex);
		for (auto res : StreamingServiceImplInstance.resources)
			res->RequestStreamingUpdate();
	}
}