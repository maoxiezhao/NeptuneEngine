#include "event.h"

namespace VulkanTest
{
	EventManager::~EventManager()
	{
		Dispatch();
	}

	void EventManager::Dispatch()
	{
		for (auto& kvp : eventDatas)
		{
			auto& eventData =* kvp.second;
			auto& handlers = eventData.handlers;
			auto& queuedEvents = eventData.queuedEvents;
			auto itr = std::remove_if(handlers.begin(), handlers.end(), [&](const EventHandler& handler) 
			{
				for (auto& event : queuedEvents)
				{
					if (!handler.memFunc(handler.handler, *event))
						return true;
				}
				return false;
			});

			handlers.erase(itr, end(handlers));
			queuedEvents.clear();
		}
	}
}