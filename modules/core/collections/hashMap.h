#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "math\hash.h"

namespace VulkanTest
{
	template<typename K, typename V, typename CustomHasher = Hasher<K>>
	struct HashMap
	{
	private:
		struct Slot 
		{
			alignas(K) U8 keyMem[sizeof(K)];
			bool valid;
		};
		Slot* keys = nullptr;
		V* values = nullptr;
		U32 capacity = 0;
		U32 size = 0;
		U32 mask = 0;

	private:
		template <typename HM, typename KK, typename VV>
		struct IteratorBase 
		{
			HM* hm;
			U32 idx;

			template <typename HM2, typename K2, typename V2>
			bool operator !=(const IteratorBase<HM2, K2, V2>& rhs) const 
			{
				ASSERT(hm == rhs.hm);
				return idx != rhs.idx;
			}

			template <typename HM2, typename K2, typename V2>
			bool operator ==(const IteratorBase<HM2, K2, V2>& rhs) const 
			{
				ASSERT(hm == rhs.hm);
				return idx == rhs.idx;
			}

			void operator++() 
			{
				const Slot* keys = hm->keys;
				for (U32 i = idx + 1, c = hm->capacity; i < c; ++i) 
				{
					if (keys[i].valid) 
					{
						idx = i;
						return;
					}
				}
				idx = hm->capacity;
			}

			KK& key() 
			{
				ASSERT(hm->keys[idx].valid);
				return *((K*)hm->keys[idx].keyMem);
			}

			const VV& value() const 
			{
				ASSERT(hm->keys[idx].valid);
				return hm->values[idx];
			}

			VV& value() 
			{
				ASSERT(hm->keys[idx].valid);
				return hm->values[idx];
			}

			VV& operator*() 
			{
				ASSERT(hm->keys[idx].valid);
				return hm->values[idx];
			}

			bool isValid() const { return idx != hm->capacity; }
		};

	public:
		using Iterator = IteratorBase<HashMap, K, V>;
		using ConstIterator = IteratorBase<const HashMap, const K, const V>;

		HashMap() = default;

		HashMap(U32 size)
		{
			init(size);
		}

		HashMap(HashMap&& rhs)
		{
			keys = rhs.keys;
			values = rhs.values;
			capacity = rhs.capacity;
			size = rhs.size;
			mask = rhs.mask;

			rhs.keys = nullptr;
			rhs.values = nullptr;
			rhs.capacity = 0;
			rhs.size = 0;
			rhs.mask = 0;
		}

		~HashMap()
		{
			for (U32 i = 0; i < capacity; i++)
			{
				if (keys[i].valid)
				{
					((K*)keys[i].keyMem)->~K();
					values[i].~V();
					keys[i].valid = false;
				}
			}
			CJING_FREE(keys);
			CJING_FREE(values);
		}

		HashMap&& move() {
			return static_cast<HashMap&&>(*this);
		}

		void operator =(HashMap&& rhs) = delete;

		void clear() 
		{
			for (U32 i = 0, c = capacity; i < c; ++i) 
			{
				if (keys[i].valid) 
				{
					((K*)keys[i].keyMem)->~K();
					values[i].~V();
					keys[i].valid = false;
				}
			}
			CJING_FREE(keys);
			CJING_FREE(values);
			init(8);
		}

		Iterator find(const K& key) 
		{
			return { this, findPos(key) };
		}

		ConstIterator find(const K& key) const
		{
			return { this, findPos(key) };
		}

		V& operator[](const K& key) 
		{
			const U32 pos = findPos(key);
			ASSERT(pos < capacity);
			return values[pos];
		}

		const V& operator[](const K& key) const 
		{
			const U32 pos = findPos(key);
			ASSERT(pos < capacity);
			return values[pos];
		}

		Iterator insert(const K& key, const V& value)
		{
			if (size >= capacity * 3 / 4) {
				grow((capacity << 1) < 8 ? 8 : capacity << 1);
			}

			// Find empty pos
			U32 pos = CustomHasher::Get(key) & mask;
			while (keys[pos].valid) ++pos;
			if (pos == capacity) 
			{
				pos = 0;
				while (keys[pos].valid) ++pos;
			}

			new (keys[pos].keyMem) K(key);
			new (&values[pos]) V(static_cast<V&&>(value));
			++size;
			keys[pos].valid = true;

			return { this, pos };
		}

		Iterator insert(const K& key, V&& value)
		{
			if (size >= capacity * 3 / 4) {
				grow((capacity << 1) < 8 ? 8 : capacity << 1);
			}

			// Find empty pos
			U32 pos = CustomHasher::Get(key) & mask;
			while (keys[pos].valid) ++pos;
			if (pos == capacity) 
			{
				pos = 0;
				while (keys[pos].valid) ++pos;
			}

			new (keys[pos].keyMem) K(key);
			new (&values[pos]) V(value);
			++size;
			keys[pos].valid = true;

			return { this, pos };
		}

		Iterator begin() 
		{
			for (U32 i = 0, c = capacity; i < c; ++i) 
			{
				if (keys[i].valid) 
					return { this, i };
			}
			return { this, capacity };
		}

		ConstIterator begin() const {
			for (U32 i = 0, c = capacity; i < c; ++i) 
			{
				if (keys[i].valid) 
					return { this, i };
			}
			return { this, capacity };
		}

		Iterator end() { return Iterator{ this, capacity }; }
		ConstIterator end() const { return ConstIterator{ this, capacity }; }

	private:
		void init(U32 capacity_)
		{
			const bool isPow2 = capacity_ && !(capacity_ & (capacity_ - 1));
			ASSERT(isPow2);

			size = 0;
			mask = capacity_ - 1;
			keys = (Slot*)CJING_MALLOC(sizeof(Slot) * (capacity_ + 1));
			values = (V*)CJING_MALLOC(sizeof(V) * capacity_);
			capacity = capacity_;
			for (U32 i = 0; i < capacity; ++i) {
				keys[i].valid = false;
			}
			keys[capacity].valid = false;
		}

		U32 findPos(const K& key) const 
		{
			U32 pos = CustomHasher::Get(key) & mask;
			if (!keys) 
			{
				ASSERT(capacity == 0);
				return 0;
			}
			while (keys[pos].valid) 
			{
				if (*((K*)keys[pos].keyMem) == key) 
					return pos;
				++pos;
			}

			if (pos != capacity) 
				return capacity;

			pos = 0;
			while (keys[pos].valid) 
			{
				if (*((K*)keys[pos].keyMem) == key) 
					return pos;
				++pos;
			}
			return capacity;
		}

		void grow(U32 newCapacity) 
		{
			HashMap<K, V, CustomHasher> tmp(newCapacity);
			if (size > 0) 
			{
				for (auto iter = begin(); iter.isValid(); ++iter)
					tmp.insert(iter.key(), static_cast<V&&>(iter.value()));
			}

			std::swap(capacity, tmp.capacity);
			std::swap(size, tmp.size);
			std::swap(mask, tmp.mask);
			std::swap(keys, tmp.keys);
			std::swap(values, tmp.values);
		}
	};
}