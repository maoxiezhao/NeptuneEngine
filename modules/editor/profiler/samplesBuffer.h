#pragma once

#ifdef CJING3D_EDITOR

#include "core\common.h"
#include "core\collections\array.h"

namespace VulkanTest
{
namespace Editor
{
	template<typename T, int Capacity>
	class SamplesBuffer
	{
	private:
		T data[Capacity];
		I32 count = 0;

	public:
		SamplesBuffer()
		{
			memset(data, 0, sizeof(T) * Capacity);
		}

		void Add(T sample)
		{
			if (count == Capacity)
			{
				for (I32 i = 1; i < count; i++)
					data[i - 1] = data[i];

				count--;
			}

			data[count++] = sample;
		}

		T& Get(I32 index)
		{
			return data[index];
		}

		const T& Get(I32 index) const
		{
			return data[index];
		}

		void Clear()
		{
			count = 0;
		}

		T* Data() {
			return data;
		}

		I32 Count()const {
			return count;
		}
	};
}
}

#endif