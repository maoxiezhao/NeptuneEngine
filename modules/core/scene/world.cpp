#include "world.h"
#include "core\utils\string.h"
#include "core\threading\jobsystem.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
	void ECSRunJob(void* ptr, void* stage, U64 pipeline)
	{
		ECS::ThreadContext* ctx = (ECS::ThreadContext*)ptr;
		ECS_ASSERT(ctx != nullptr);

		if (ctx->payload == nullptr)
			ctx->payload = CJING_NEW(Jobsystem::JobHandle);

		Jobsystem::Run(nullptr, [stage, pipeline](void*) {
			ECS::RunPipelineThread((ECS::Stage*)stage, pipeline);
			}, (Jobsystem::JobHandle*)ctx->payload);
	}

	void ECSWaitJob(void* ptr)
	{
		ECS::ThreadContext* ctx = (ECS::ThreadContext*)ptr;
		ECS_ASSERT(ctx != nullptr);

		if (ctx->payload != nullptr)
		{
			Jobsystem::Wait((Jobsystem::JobHandle*)ctx->payload);
			CJING_SAFE_DELETE(ctx->payload);
		}
	}

	static void* ECSMalloc(size_t size)
	{
		return CJING_MALLOC(size);
	}

	static void* ECSCalloc(size_t size)
	{
		if (size == 96)
			int a = 0;
		void* ret = CJING_MALLOC(size);
		memset(ret, 0, size);
		return ret;
	}

	static void* ECSRealloc(void* ptr, size_t size)
	{
		return CJING_REMALLOC(ptr, size);
	}

	static void ECSFree(void* ptr)
	{
		CJING_FREE(ptr);
	}


	static char* EcsSystemAPIStrdup(const char* str)
	{
		if (str)
		{
			int len = strlen(str);
			char* result = (char*)CJING_MALLOC(len + 1);
			ECS_ASSERT(result != NULL);
			strcpy_s(result, len + 1, str);
			return result;
		}
		else
		{
			return NULL;
		}
	}

	void GetValidEntityName(World* world, char(&out)[64])
	{
		while (world->FindEntity(out) != ECS::INVALID_ENTITYID)
			CatString(out, "_");
	}

	World::World() :
		ECS::World()
	{
		I32 count = std::clamp((I32)(Platform::GetCPUsCount() * 0.5f), 1, 12);
		Logger::Info("Create ecs stage count %d", count);
		SetThreads(count);
	}

	World::~World()
	{
	}

	void World::SetupWorld()
	{
		ECS::EcsSystemAPI api = {};
		api.malloc_ = ECSMalloc;
		api.calloc_ = ECSCalloc;
		api.realloc_ = ECSRealloc;
		api.free_ = ECSFree;
		api.thread_run_ = ECSRunJob;
		api.thread_sync_ = ECSWaitJob;
		api.strdup_ = EcsSystemAPIStrdup;
		ECS::SetSystemAPI(api);
	}

	ECS::Entity World::CreateEntity(const char* name)
	{
		ECS::Entity result;
		if (name != nullptr && name[0] != 0)
		{
			char tmp[64];
			CopyString(Span(tmp), name);
			GetValidEntityName(this, tmp);
			result = Entity(tmp);
		}
		else
		{
			result = Entity(nullptr);
		}

		entityCreated.Invoke(result);
		return result;
	}

	void World::DeleteEntity(ECS::Entity entity)
	{
		entityDestroyed.Invoke(entity);
		entity.Destroy();
	}

	IScene* World::GetScene(const char* name) const
	{
		for (auto& scene : scenes)
		{
			if (EqualString(scene->GetPlugin().GetName(), name))
				return scene.Get();
		}
		return nullptr;
	}

	void World::AddScene(UniquePtr<IScene>&& scene)
	{
		scenes.push_back(scene.Move());
	}

	std::vector<UniquePtr<IScene>>& World::GetScenes()
	{
		return scenes;
	}
}