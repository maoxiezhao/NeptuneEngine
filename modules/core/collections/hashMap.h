#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "math\hash.h"

namespace VulkanTest
{
	template <typename Key>
	struct HashMapHashFunc
	{
		static U32 Get(const Key& key);
	};

	template<>
	struct HashMapHashFunc<U64>
	{
		static U32 Get(U64 key) 
		{
			// https://xoshiro.di.unimi.it/splitmix64.c
			U64 x = key;
			x ^= x >> 30;
			x *= 0xbf58476d1ce4e5b9U;
			x ^= x >> 27;
			x *= 0x94d049bb133111ebU;
			x ^= x >> 31;
			return U32((x >> 32) ^ x);
		}
	};


	template<>
	struct HashMapHashFunc<I32>
	{
		static U32 Get(I32 key) 
		{
			// https://nullprogram.com/blog/2018/07/31/
			U32 x = key;
			x ^= x >> 16;
			x *= 0x7feb352dU;
			x ^= x >> 15;
			x *= 0x846ca68bU;
			x ^= x >> 16;
			return x;
		}
	};

	template<>
	struct HashMapHashFunc<U32>
	{
		static U32 Get(const U32& key) 
		{
			U32 x = key;
			x ^= x >> 16;
			x *= 0x7feb352dU;
			x ^= x >> 15;
			x *= 0x846ca68bU;
			x ^= x >> 16;
			return x;
		}
	};

	template<typename T>
	struct HashMapHashFunc<T*>
	{
		static U32 Get(const void* key)
		{
			U64 x = (U64)key;
			x ^= x >> 30;
			x *= 0xbf58476d1ce4e5b9U;
			x ^= x >> 27;
			x *= 0x94d049bb133111ebU;
			x ^= x >> 31;
			return U32((x >> 32) ^ x);
		}
	};

	template <typename T>
	struct HashFuncDirect {
		static U32 Get(const T& key) { return key; }
	};

	template<typename K, typename V, typename CustomHasher = HashMapHashFunc<K>>
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

		bool tryGet(const K& key, V& value)
		{
			auto it = find(key);
			if (!it.isValid())
				return false;

			value = it.value();
			return true;
		}

		bool contains(const K& key)
		{
			return find(key).isValid();
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
			new (&values[pos]) V(value);
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
			new (&values[pos]) V(static_cast<V&&>(value));
			++size;
			keys[pos].valid = true;

			return { this, pos };
		}

		Iterator emplace(const K& key)
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
			new (&values[pos]) V();
			++size;
			keys[pos].valid = true;

			return { this, pos };
		}

		template <typename F>
		void eraseIf(F predicate) 
		{
			for (U32 i = 0; i < capacity; ++i) 
			{
				if (!keys[i].valid) 
					continue;

				if (!predicate(values[i]))
					continue;

				((K*)keys[i].keyMem)->~K();
				values[i].~V();
				keys[i].valid = false;
				--size;

				U32 pos = (i + 1) & mask;
				while (keys[pos].valid)
				{
					rehash(pos);
					pos = (pos + 1) % capacity;
				}
			}
		}

		void erase(const Iterator& iter) 
		{
			ASSERT(iter.isValid());

			U32 pos = iter.idx;
			((K*)keys[pos].keyMem)->~K();
			values[pos].~V();
			keys[pos].valid = false;
			--size;

			pos = (pos + 1) & mask;
			while (keys[pos].valid) 
			{
				rehash(pos);
				pos = (pos + 1) % capacity;
			}
		}

		void erase(const K& key) 
		{
			const U32 pos = findPos(key);
			if (keys[pos].valid) 
				erase(Iterator{ this, pos });
		}

		bool empty() const { return size == 0; }
		U32 count() const { return size; }

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

		U32 findEmptySlot(const K& key, U32 endPos) const 
		{
			U32 pos = CustomHasher::Get(key) & mask;
			while (keys[pos].valid && pos != endPos) 
				++pos;

			if (pos == capacity) 
			{
				pos = 0;
				while (keys[pos].valid && pos != endPos) 
					++pos;
			}
			return pos;
		}

		void rehash(U32 pos) 
		{
			K& key = *((K*)keys[pos].keyMem);
			const U32 rehashedPos = findEmptySlot(key, pos);
			if (rehashedPos != pos) 
			{
				new (keys[rehashedPos].keyMem) K(key);
				new (&values[rehashedPos]) V(static_cast<V&&>(values[pos]));

				((K*)keys[pos].keyMem)->~K();
				values[pos].~V();
				keys[pos].valid = false;
				keys[rehashedPos].valid = true;
			}
		}

	};
}