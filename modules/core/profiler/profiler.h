#pragma once

#include "core\common.h"
#include "core\platform\platform.h"
#include "core\platform\sync.h"
#include "core\platform\atomic.h"
#include "core\utils\string.h"

namespace VulkanTest
{
	namespace Profiler
	{
		enum class BlockType : U8
		{
			CPU_BLOCK,
			FIBER,
			COUNT
		};

		struct Block
		{
			const char* name;
			I32 id;
			BlockType type;
			F64 startTime;
			F64 endTime;
			I32 depth;
		};

		// Simple events ring-buffer
		struct BlockBuffer
		{
		private:
			Profiler::Block* data;
			I32 capacity;
			I32 mask;
			I32 head;
			I32 count;

		public:
			BlockBuffer();
			~BlockBuffer();

			Profiler::Block* Get(I32 index)const
			{
				ASSERT(index >= 0 && index < capacity);
				return &data[index];
			}

			void Clear()
			{
				head = 0;
				count = 0;
			}

			I32 Add()
			{
				const I32 index = head;
				head = (head + 1) & mask;
				count = std::min(count + 1, capacity);
				return index;
			}

			I32 Count()const {
				return count;
			}

			void GetBlocks(std::vector<Block>& blocks);

		public:
			struct Iterator
			{
				friend BlockBuffer;

			private:
				BlockBuffer* buffer;
				I32 index;

				Iterator(BlockBuffer* buffer_, const I32 index_)
					: buffer(buffer_)
					, index(index_)
				{
				}

				Iterator(const Iterator& i) = default;

			public:
				FORCE_INLINE I32 Index() const
				{
					return index;
				}

				FORCE_INLINE Block& Block() const
				{
					return *buffer->Get(index);
				}

				bool IsEnd() const
				{
					return index == buffer->head;
				}

				bool IsNotEnd() const
				{
					return index != buffer->head;
				}

				FORCE_INLINE bool operator==(const Iterator& v) const
				{
					return buffer == v.buffer && index == v.index;
				}

				FORCE_INLINE bool operator!=(const Iterator& v) const
				{
					return buffer != v.buffer || index != v.index;
				}

			public:
				Iterator& operator++()
				{
					ASSERT(buffer);
					index = (index + 1) & buffer->mask;
					return *this;
				}

				Iterator operator++(int)
				{
					ASSERT(buffer);
					Iterator temp = *this;
					index = (index + 1) & buffer->mask;
					return temp;
				}

				Iterator& operator--()
				{
					ASSERT(buffer);
					index = (index - 1) & buffer->mask;
					return *this;
				}

				Iterator operator--(int)
				{
					ASSERT(buffer);
					Iterator temp = *this;
					index = (index - 1) & buffer->mask;
					return temp;
				}
			};

			FORCE_INLINE Iterator Begin()
			{
				return Iterator(this, (head - count) & mask);
			}

			FORCE_INLINE Iterator Last()
			{
				ASSERT(count > 0);
				return Iterator(this, (head - 1) & mask);
			}

			FORCE_INLINE Iterator End()
			{
				return Iterator(this, head);
			}
		};

		// Thread profiler
		struct Thread
		{
			StaticString<64> name;
			Mutex mutex;
			Platform::ThreadID threadID = 0;
			I32 depth = 0;
			BlockBuffer entBuffer;
			std::vector<I32> blockStack;

			Block* BeginBlock();
			void EndBlock(I32 index);
			void GetBlocks(std::vector<Block>& blocks);
		};

		void SetThreadName(const char* name);
		void BeginFrame();
		void EndFrame();
		void BeginBlock(const char* name);
		void EndBlock();
		void Enable(bool enabled);
		void BeginFiberWait();
		void EndFiberWait();
		std::vector<Thread*>& GetThreads();

		struct Scope
		{
		public:
			explicit Scope(const char* name) { 
				BeginBlock(name); 
			}

			~Scope() { 
				EndBlock();
			}
		};
	}

#define PROFILER_CONCAT2(a, b) a ## b
#define PROFILER_CONCAT(a, b) PROFILER_CONCAT2(a, b)
#define PROFILE_BLOCK(name) Profiler::Scope PROFILER_CONCAT(profile_scope, __LINE__)(name);
#define PROFILE_FUNCTION() Profiler::Scope  profile_scope(__FUNCTION__);
}