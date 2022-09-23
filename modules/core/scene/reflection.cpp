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
			firstScene = nullptr;
		}
	};

	static Context& GetContext()
	{
		static Context ctx;
		return ctx;
	}

	ComponentType GetComponentType(const char* id)
	{
		auto& ctx = GetContext();
		const RuntimeHash nameHash(id);
		for (I32 i = 0; i < ctx.comps.size(); i++)
		{
			if (ctx.comps[i].name == nameHash)
				return { i };
		}

		auto& newComp = ctx.comps.emplace();
		newComp.name = nameHash;
		return { (I32)ctx.comps.size() - 1 };
	}

	SceneMeta::~SceneMeta()
	{
		for (auto cmp : cmps)
			CJING_SAFE_DELETE(cmp);
	}

	ComponentMeta::~ComponentMeta()
	{
		for (auto prop : props)
		{
			for (auto attr : prop->attributes)
				CJING_SAFE_DELETE(attr);

			CJING_SAFE_DELETE(prop);
		}
	}

	void ComponentMeta::Visit(IPropertyMetaVisitor& visitor) const
	{
		for (const auto prop : props)
			prop->Visit(visitor);
	}

	Builder BuildScene(const char* name)
	{
		Builder builder = {};
		Context& ctx = GetContext();
		builder.scene->next = ctx.firstScene;
		ctx.firstScene = builder.scene;
		builder.scene->name = name;
		return builder;
	}

	ComponentMeta* GetComponent(ComponentType compType)
	{
		return GetContext().comps[compType.index].meta;
	}

	Span<const RegisteredComponent> GetComponents()
	{
		return Span(GetContext().comps.data(), GetContext().comps.size());
	}

	Builder::Builder()
	{
		scene = CJING_NEW(SceneMeta)();
	}

	Builder& Builder::ColorAttribute()
	{
		auto attr = CJING_NEW(Reflection::ColorAttribute)();
		lastProp->attributes.push_back(attr);
		return *this;
	}

	void Builder::RegisterCmp(ComponentMeta* cmp)
	{
		GetContext().comps[cmp->compType.index].meta = cmp;
		GetContext().comps[cmp->compType.index].name = RuntimeHash(cmp->name);
		GetContext().comps[cmp->compType.index].scene = RuntimeHash(scene->name);
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