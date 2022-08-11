#pragma once

#include "core\common.h"
#include "core\memory\memory.h"

#define MOODYCAMEL_EXCEPTIONS_ENABLED 0
#include "utility\concurrentqueue.h"

namespace VulkanTest
{
    struct ConcurrentQueueSettings : public moodycamel::ConcurrentQueueDefaultTraits
    {
        // Use bigger blocks
        static const size_t BLOCK_SIZE = 256;

        static inline void* malloc(size_t size)
        {
            return CJING_MALLOC(size);
        }

        static inline void free(void* ptr)
        {
            return CJING_FREE(ptr);
        }
    };

    // Lock-free implementation of thread-safe queue.
    // Based on: https://github.com/cameron314/concurrentqueue
    template<typename T>
    class ConcurrentQueue : public moodycamel::ConcurrentQueue<T, ConcurrentQueueSettings>
    {
    public:
        typedef moodycamel::ConcurrentQueue<T, ConcurrentQueueSettings> Base;

        FORCE_INLINE I32 Count() const
        {
            return static_cast<I32>(Base::size_approx());
        }

        FORCE_INLINE void Add(T& item)
        {
            enqueue(item);
        }
    };
}