#pragma once

#include "core\common.h"
#include "core\collections\concurrentqueue.hpp"

namespace VulkanTest
{
	template<typename T>
	class ConcurrentTaskQueue : public ConcurrentQueue<T*>
	{
	public:
		FORCE_INLINE void Add(T* item)
		{
			ConcurrentQueue<T*>::enqueue(item);
		}

		void CancelAll()
		{
			T* tasks[16];
			std::size_t count;
			while ((count = ConcurrentQueue<T*>::try_dequeue_bulk(tasks, 16)) != 0)
			{
				for (std::size_t i = 0; i != count; i++)
				{
					tasks[i]->Cancel();
				}
			}
		}
	};
}