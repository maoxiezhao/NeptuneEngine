#pragma once

#include "core\common.h"
#include "core\scene\world.h"
#include "core\resource\resource.h"
#include "meta\meta.hpp"
#include "meta\factory.hpp"
#include "meta\policy.hpp"

namespace VulkanTest
{
namespace Reflection
{
	template <typename T> struct ResultOf;
	template <typename R, typename C, typename... Args> struct ResultOf<R(C::*)(Args...)> { using Type = R; };
	template <typename R, typename C, typename... Args> struct ResultOf<R(C::*)(Args...) const> { using Type = R; };
	template <typename R, typename C> struct ResultOf<R(C::*)> { using Type = R; };

	template <typename T> struct ClassOf;
	template <typename R, typename C, typename... Args> struct ClassOf<R(C::*)(Args...)> { using Type = C; };
	template <typename R, typename C, typename... Args> struct ClassOf<R(C::*)(Args...)const > { using Type = C; };
	template <typename R, typename C> struct ClassOf<R(C::*)> { using Type = C; };

	template <typename T> struct ArgsCount;
	template <typename R, typename C, typename... Args> struct ArgsCount<R(C::*)(Args...)> { static constexpr U32 value = sizeof...(Args); };
	template <typename R, typename C, typename... Args> struct ArgsCount<R(C::*)(Args...)const > { static constexpr U32 value = sizeof...(Args); };
	template <typename R, typename C> struct ArgsCount<R(C::*)> { static constexpr U32 value = 0; };

	struct IAttribute
	{
		enum Type : U32
		{
			RESOURCE,
			COUNT
		};

		virtual ~IAttribute() {}
		virtual Type GetType() const = 0;
	};

	struct ResourceAttribute : IAttribute
	{
		ResourceAttribute(ResourceType type) : resType(type) {}
		ResourceAttribute() {}
		Type GetType() const override { return RESOURCE; }

		ResourceType resType;
	};

	struct PropertyMetaBase
	{
		const char* name;
		Array<IAttribute*> attributes;
	};

	template <typename T>
	struct PropertyMeta : PropertyMetaBase
	{
		typedef T(*Getter)(IScene*, ECS::EntityID, U32);
		typedef void (*Setter)(IScene*, ECS::EntityID, U32, const T&);

		virtual T Get(IScene* scene, ECS::EntityID entity, U32 idx) const {
			return getter(scene, entity, idx);
		}
		virtual void Set(IScene* scene, ECS::EntityID entity, U32 idx, T val) const {
			setter(scene, entity, idx, val);
		}

		virtual bool IsReadonly() const { return setter == nullptr; }

		Setter setter = nullptr;
		Getter getter = nullptr;
	};

	struct RegisteredComponent 
	{
		StringID name;
		StringID scene;
		struct ComponentMeta* meta = nullptr;
	};

	using CreateComponent = ECS::EntityID (*)(IScene*, const char*);
	using DestroyComponent = void (*)(IScene*, ECS::EntityID);

	struct ComponentMeta
	{
		const char* name;
		ECS::EntityID compID;
		CreateComponent creator;
		DestroyComponent destroyer;

		Array<PropertyMetaBase*> props;
	};

	struct SceneMeta
	{
		const char* name;
		SceneMeta* next = nullptr;
		Array<ComponentMeta*> cmps;
	};

	struct VULKAN_TEST_API Builder
	{
	public:
		World* world = nullptr;
		SceneMeta* scene = nullptr;

		Builder();

		template<typename C, auto Creator, auto Destroyer>
		Builder& Component(const char* name)
		{
			auto creator = [](IScene* scene, const char* name) { return (scene->*static_cast<ECS::EntityID (IScene::*)(const char*)>(Creator))(name); };
			auto destroyer = [](IScene* scene, ECS::EntityID e) { (scene->*static_cast<void (IScene::*)(ECS::EntityID)>(Destroyer))(e); };
		
			ComponentMeta* cmp = CJING_NEW(ComponentMeta)();
			cmp->name = name;
			cmp->compID = world->GetComponentID<C>();
			cmp->creator = creator;
			cmp->destroyer = destroyer;
			RegisterCmp(cmp);

			return *this;
		}

		template <auto Getter, auto Setter = nullptr>
		Builder& Prop(const char* name) 
		{
			using T = typename ResultOf<decltype(Getter)>::Type;
		
			auto* p = CJING_NEW(PropertyMeta<T>)();
			p->name = name;
			if constexpr (Setter == nullptr) 
			{
				p->setter = nullptr;
			}
			else 
			{
				p->setter = [](IScene* scene, ECS::EntityID e, U32 idx, const T& value) {
					using C = typename ClassOf<decltype(Setter)>::Type;
					if constexpr (ArgsCount<decltype(Setter)>::value == 2)
						(static_cast<C*>(scene)->*Setter)(e, value);
					else 
						(static_cast<C*>(scene)->*Setter)(e, idx, value);
				};
			}

			p->getter = [](IScene* scene, ECS::EntityID e, U32 idx) -> T 
			{
				using C = typename ClassOf<decltype(Getter)>::Type;
				if constexpr (ArgsCount<decltype(Getter)>::value == 1) {
					return (static_cast<C*>(scene)->*Getter)(e);
				}
				else {
					return (static_cast<C*>(scene)->*Getter)(e, idx);
				}
			};

			AddProp(p);	
			return *this;
		}

	private:
		void RegisterCmp(ComponentMeta* cmp);
		void AddProp(PropertyMetaBase* p);

		ComponentMeta* lastComp = nullptr;
		PropertyMetaBase* lastProp = nullptr;
	};

	VULKAN_TEST_API Builder BuildScene(World* world, const char* name);
}
}