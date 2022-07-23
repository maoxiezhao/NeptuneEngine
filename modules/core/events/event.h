#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\collections\intrusiveHashMap.hpp"
#include "math\compileTimeHash.h"

namespace VulkanTest
{
	class Event;
	using EventType = HashValue;

	namespace EventImpl
	{
		template <typename Return, typename T, typename EventType, Return(T::* callback)(const EventType& e)>
		Return MemberFunctionInvoker(void* object, const Event& e)
		{
			return (static_cast<T*>(object)->*callback)(static_cast<const EventType&>(e));
		}

		template <typename Return, typename EventType, Return(*callback)(const EventType& e)>
		Return StaticFunctionInvoker(void* object, const Event& e)
		{
			return callback(static_cast<const EventType&>(e));
		}
	}

#define GET_EVENT_TYPE_HASH(x) ::VulkanTest::CompileTimeFnv1(#x)

#define DEFINE_EVENT_TYPE(x) \
enum class EventTypeWrapper : ::VulkanTest::EventType { \
	typeID = GET_EVENT_TYPE_HASH(x) \
}; \
static inline constexpr ::VulkanTest::EventType GetTypeID() { \
	return ::VulkanTest::EventType(EventTypeWrapper::typeID); \
}
	class Event
	{
	public:
		virtual ~Event() = default;
		Event() = default;
	};

	class EventManager
	{
	public:
		~EventManager();

		static EventManager& Instance()
		{
			static EventManager inst;
			return inst;
		}

		template<typename T, typename EventType, bool (T::* memFunc)(const EventType&)>
		void Register(T* handler)
		{
			static constexpr HashValue hashCode = EventType::GetTypeID();
			auto& eventData = eventDatas[hashCode];
			if (eventData.dispatching)
				eventData.recursiveHandlers.push_back({ EventImpl::MemberFunctionInvoker<bool, T, EventType, memFunc>, handler });
			else
				eventData.handlers.push_back({ EventImpl::MemberFunctionInvoker<bool, T, EventType, memFunc>, handler });
		}

		template<typename EventType, bool (*func)(const EventType&)>
		void Register()
		{
			static constexpr HashValue typeID = EventType::GetTypeID();
			auto& eventData = eventDatas[typeID];
			if (eventData.dispatching)
				eventData.recursiveHandlers.push_back({ EventImpl::StaticFunctionInvoker<bool, EventType, func>, nullptr });
			else
				eventData.handlers.push_back({ EventImpl::StaticFunctionInvoker<bool, EventType, func>, nullptr });
		}

		template<typename T>
		void Unregister(T* handler)
		{

		}

		template<typename T, typename... Args>
		void Enqueue(Args&&... args)
		{
			static constexpr HashValue typeID = T::GetTypeID();
			auto& eventData = eventDatas[typeID];
			auto ent = std::unique_ptr<Event>(new T(std::forward<Args>(args)...));
			eventData.queuedEvents.emplace_back(std::move(ent));
		}

		void Dispatch();

	private:
		struct EventHandler
		{
			bool (*memFunc)(void* object, const Event& event);
			void* handler;
		};

		struct EventTypeData : Util::IntrusiveHashMapEnabled<EventTypeData>
		{
			std::vector<EventHandler> handlers;
			std::vector<EventHandler> recursiveHandlers;
			std::vector<std::unique_ptr<Event>> queuedEvents;
			bool dispatching = false;
		};
		Util::IntrusiveHashMap<EventTypeData> eventDatas;
	};
}