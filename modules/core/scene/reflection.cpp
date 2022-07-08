#include "reflection.h"

namespace VulkanTest
{
namespace Reflection
{
	struct Context
	{
		SceneMeta* firstScene = nullptr;
		Array<RegisteredComponent> comps;
	};

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

	Builder::Builder()
	{
		scene = CJING_NEW(SceneMeta)();
	}

	void Builder::RegisterCmp(ComponentMeta* cmp)
	{
		lastComp = cmp;
		scene->cmps.push_back(cmp);
	}

	void Builder::AddProp(PropertyMetaBase* p)
	{
		ASSERT(lastComp);
		lastComp->props.push_back(p);
		lastProp = p;
	}
}
}