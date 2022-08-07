#include "reflection.h"
#include "core\collections\hashMap.h"

namespace VulkanTest
{
namespace Reflection
{
	struct Context
	{
		SceneMeta* firstScene = nullptr;
		Array<RegisteredComponent> comps;
		HashMap<ECS::EntityID, RegisteredComponent*> compMap;

		~Context()
		{
			SceneMeta* cur = nullptr;
			SceneMeta* next = firstScene;
			while (next != nullptr)
			{
				cur = next;
				next = next->next;

				CJING_DELETE(cur);
			}
		}
	};

	SceneMeta::~SceneMeta()
	{
		for (auto cmp : cmps)
			CJING_SAFE_DELETE(cmp);
	}

	ComponentMeta::~ComponentMeta()
	{
		for (auto prop : props)
			CJING_SAFE_DELETE(prop);
	}

	static Context& GetContext() 
	{
		static Context ctx;
		return ctx;
	}

	Builder BuildScene(World* world, const char* name)
	{
		Builder builder = {};
		Context& ctx = GetContext();
		builder.world = world;
		builder.scene->next = ctx.firstScene;
		ctx.firstScene = builder.scene;
		builder.scene->name = name;
		return builder;
	}

	ComponentMeta* GetComponent(ECS::EntityID compID)
	{
		auto it = GetContext().compMap.find(compID);
		return it.isValid() ? it.value()->meta : nullptr;
	}

	Span<const RegisteredComponent> GetComponents()
	{
		return Span(GetContext().comps.data(), GetContext().comps.size());
	}

	Builder::Builder()
	{
		scene = CJING_NEW(SceneMeta)();
	}

	void Builder::RegisterCmp(ComponentMeta* cmp)
	{
		lastComp = cmp;
		scene->cmps.push_back(cmp);

		auto& meta = GetContext().comps.emplace();
		meta.meta = cmp;
		meta.name = StringID(cmp->name);
		meta.scene = StringID(scene->name);
		GetContext().compMap.insert(cmp->compID, &meta);
	}

	void Builder::AddProp(PropertyMetaBase* p)
	{
		ASSERT(lastComp);
		lastComp->props.push_back(p);
		lastProp = p;
	}
}
}