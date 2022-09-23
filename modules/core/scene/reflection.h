#pragma once

#include "core\common.h"
#include "core\scene\world.h"

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
			COLOR,
			COUNT
		};

		virtual ~IAttribute() {}
		virtual Type GetType() const = 0;
	};

	struct ColorAttribute : IAttribute
	{
		Type GetType() const override { return COLOR; }
	};

	struct PropertyMetaBase
	{
		const char* name;
		Array<IAttribute*> attributes;

		virtual void Visit(struct IPropertyMetaVisitor& visitor) const = 0;
	};

	template <typename T>
	struct PropertyMeta : PropertyMetaBase
	{
		typedef T(*Getter)(IScene*, ECS::Entity, U32);
		typedef void (*Setter)(IScene*, ECS::Entity, U32, const T&);

		virtual T Get(IScene* scene, ECS::Entity entity, U32 idx) const {
			return getter(scene, entity, idx);
		}
		virtual void Set(IScene* scene, ECS::Entity entity, U32 idx, T val) const {
			setter(scene, entity, idx, val);
		}

		virtual bool IsReadonly() const { return setter == nullptr; }

		void Visit(struct IPropertyMetaVisitor& visitor) const override;

		Setter setter = nullptr;
		Getter getter = nullptr;
	};

	struct IPropertyMetaVisitor
	{
		virtual ~IPropertyMetaVisitor() {}
		virtual void Visit(const PropertyMeta<F32>& prop) = 0;
		virtual void Visit(const PropertyMeta<F32x3>& prop) = 0;
	};

	template <typename T>
	void PropertyMeta<T>::Visit(IPropertyMetaVisitor& visitor) const
	{
		visitor.Visit(*this);
	}

	struct RegisteredComponent 
	{
		StringID name;
		StringID scene;
		struct ComponentMeta* meta = nullptr;
	};

	using CreateComponent = ECS::Entity (*)(IScene*, ECS::Entity);
	using DestroyComponent = void (*)(IScene*, ECS::Entity);

	struct ComponentMeta
	{
		const char* name;
		const char* icon = "";
		ECS::EntityID compID;
		CreateComponent creator;
		DestroyComponent destroyer;

		Array<PropertyMetaBase*> props;

	public:
		~ComponentMeta();

		void Visit(IPropertyMetaVisitor& visitor) const;
	};

	struct SceneMeta
	{
		~SceneMeta();

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
			auto creator = [](IScene* scene, ECS::Entity e) { return (scene->*static_cast<ECS::Entity (IScene::*)(ECS::Entity)>(Creator))(e); };
			auto destroyer = [](IScene* scene, ECS::Entity e) { (scene->*static_cast<void (IScene::*)(ECS::Entity)>(Destroyer))(e); };
		
			ComponentMeta* cmp = CJING_NEW(ComponentMeta)();
			cmp->name = name;
			cmp->compID = world->GetComponentID<C>();
			cmp->creator = creator;
			cmp->destroyer = destroyer;
			RegisterCmp(cmp);

			return *this;
		}

		Builder& Icon(const char* icon)
		{
			scene->cmps.back()->icon = icon;
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
				p->setter = [](IScene* scene, ECS::Entity e, U32 idx, const T& value) {
					using C = typename ClassOf<decltype(Setter)>::Type;
					if constexpr (ArgsCount<decltype(Setter)>::value == 2)
						(static_cast<C*>(scene)->*Setter)(e, value);
					else 
						(static_cast<C*>(scene)->*Setter)(e, idx, value);
				};
			}

			p->getter = [](IScene* scene, ECS::Entity e, U32 idx) -> T 
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

		template<typename C, auto PropGetter>
		Builder& VarProp(const char* name)
		{
			using T = typename ResultOf<decltype(PropGetter)>::Type;
			auto* p = CJING_NEW(PropertyMeta<T>)();
			p->name = name;

			p->setter = [](IScene* scene, ECS::Entity e, U32 idx, const T& value) {
				C* comp = e.GetMut<C>();
				ASSERT(comp != nullptr);
				auto& v = (*comp).*PropGetter;
				v = value;
			};

			p->getter = [](IScene* scene, ECS::Entity e, U32 idx) -> T
			{
				const C* comp = e.Get<C>();
				ASSERT(comp != nullptr);
				auto& v = (*comp).*PropGetter;
				return static_cast<T>(v);
			};
			AddProp(p);
			return *this;
		}

		Builder& ColorAttribute();

	private:
		void RegisterCmp(ComponentMeta* cmp);
		void AddProp(PropertyMetaBase* p);

		ComponentMeta* lastComp = nullptr;
		PropertyMetaBase* lastProp = nullptr;
	};

	VULKAN_TEST_API Builder BuildScene(World* world, const char* name);
	VULKAN_TEST_API ComponentMeta* GetComponent(ECS::EntityID compID);
	VULKAN_TEST_API Span<const RegisteredComponent> GetComponents();
	VULKAN_TEST_API void ClearReflections();
}
}