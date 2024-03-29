#pragma once

#include "core\common.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
    template<typename T>
    class Array
    {
    public:
        Array() = default;
        ~Array()
        {
            if (data_ != nullptr)
            {
                DestructData(data_, data_ + size_);
                CJING_FREE_ALIGN(data_);
            }
        }

        Array(const Array& rhs) = delete;
        void operator=(const Array& rhs) = delete;
        
        Array(Array&& rhs) {
            swap(std::move(rhs));
        }
        void operator=(Array&& rhs)
        {
            if (this == &rhs) return;
            if (data_ != nullptr)
            {
                DestructData(data_, data_ + size_);
                CJING_FREE_ALIGN(data_);
            }

            std::swap(size_, rhs.size_);
            std::swap(capacity_, rhs.capacity_);
            std::swap(data_, rhs.data_);
        }

        void swap(Array<T>&& rhs)
        {
            std::swap(size_, rhs.size_);
            std::swap(capacity_, rhs.capacity_);
            std::swap(data_, rhs.data_);
        }

        T* begin() const { 
            return data_;
        }
        T* end() const {
            return data_ != nullptr ? data_ + size_ : nullptr;
        }

        T& front()
        {
            ASSERT(size_ > 0);
            return data_[0];
        }

        T& back() 
        {
            ASSERT(size_ > 0);
            return data_[size_ - 1];
        }

        void pop_front()
        {
            eraseAt(0);
        }

        void pop_back()
        {
            if (size_ > 0)
            {
                data_[size_ - 1].~T();
                size_--;
            }
        }

        I32 indexOf(const T& v)
        {
            for (U32 i = 0; i < size_; i++)
            {
                if (data_[i] == v)
                    return i;
            }
            return -1;
        }

        void push_back(T&& value)
        {
            U32 oldSize = size_;
            if (oldSize == capacity_)
                Grow();
            new (data_ + oldSize) T(static_cast<T&&>(value));    // Move construct
            size_ = oldSize + 1;
        }

        void push_back(const T& value)
        {
            U32 oldSize = size_;
            if (oldSize == capacity_)
                Grow();
            new (data_ + oldSize) T(value);    // copy construct
            size_ = oldSize + 1;
        }

        template <typename... Args> 
        T& emplace(Args&&... args) 
        {
            if (size_ == capacity_) 
                Grow();
            new ((char*)(data_ + size_)) T(static_cast<Args&&>(args)...);
            ++size_;
            return data_[size_ - 1];
        }

        void Insert(U32 index, const T& value) {
            EmplaceAt(index, value);
        }

        void Insert(U32 index, T&& value) {
            EmplaceAt(index, static_cast<T&&>(value));
        }

        template <typename... Args>
        T& EmplaceAt(U32 index, Args&&... args)
        {
            if constexpr (__is_trivially_copyable(T)) 
            {
                if (size_ == capacity_)
                    Grow();
                memmove(&data_[index + 1], &data_[index], sizeof(data_[index]) * (size_ - index));
                new (data_ + index) T(static_cast<Args&&>(args)...);
            }
            else
            {
                if (size_ == capacity_)
                {
                    U32 newCapacity = capacity_ == 0 ? 4 : capacity_ * 2;
                    T* oldData = data_;
                    data_ = static_cast<T*>(CJING_MALLOC_ALIGN(newCapacity * sizeof(T), alignof(T)));
                    MoveData(data_, oldData, index);
                    MoveData(data_ + index + 1, oldData + index, size_ - index);
                    CJING_FREE_ALIGN(oldData);
                    capacity_ = newCapacity;
                }
                else
                {
                    MoveData(data_ + index + 1, data_ + index, size_ - index);
                }
                new (data_ + index) T(static_cast<Args&&>(args)...);
            }
            size_++;
            return data_[index];
        }

        int indexOf(const T& item) const
        {
            for (U32 i = 0; i < size_; ++i) 
            {
                if (data_[i] == item) {
                    return i;
                }
            }
            return -1;
        }

        void erase(const T& value)
        {
            for(U32 i = 0; i < size_; i++)
            {
                if (data_[i] == value)
                {
                    eraseAt(i);
                    return;
                }
            }
        }

        // Keep items order
        void eraseAt(U32 index)
        {
            ASSERT(index < size_);
            data_[index].~T();
            if (index < size_ - 1)
            {
                if constexpr (__is_trivially_copyable(T)) 
                {
                    memmove(data_ + index, data_ + index + 1, sizeof(T) * (size_ - index - 1));
                } 
                else 
                {
                    for (U32 i = index; i < size_ - 1; ++i)
                     {
                        new (&data_[i]) T(static_cast<T&&>(data_[i + 1]));
                        data_[i + 1].~T();
                    }
                }
            }
            size_--;
        }

        void swapAndPop(U32 index)
        {
            ASSERT(index < size_);
            if constexpr (__is_trivially_copyable(T)) 
            {
                // Copy the last item to the current item
                memmove(data_ + index, data_ + size_ - 1, sizeof(T));
            } 
            else 
            {
                data_[index].~T();
                if (index != size_ - 1)
                {
                    // Move the last item to the current item
                    new (data_ + index) T(static_cast<T&&>(data_[size_ - 1])); 
                    data_[size_ - 1].~T();
                }
            }
            size_--;
        }

        void swapAndPopItem(const T& item)
        {
            for(U32 i = 0; i < size_; i++)
            {
                if (data_[i] == item)
                {
                    swapAndPop(i);
                    return;
                }
            }
        }

        void clear()
        {
            DestructData(data_, data_ + size_);
            size_ = 0;
        }

        void clearDelete()
        {
            for (U32 i = 0; i < size_; i++)
                CJING_SAFE_DELETE(data_[i]);
            clear();
        }

        Array<T> copy() const 
        {
            Array<T> res;
            if (size_ == 0) 
                return res;

            res.data_ = static_cast<T*>(CJING_MALLOC_ALIGN(size_ * sizeof(T), alignof(T)));
            res.capacity_ = size_;
            res.size_ = size_;
            for (U32 i = 0; i < size_; ++i) {
                new (res.data_ + i) T(data_[i]);
            }
            return res;
        }

        void release()
        {
            clear();
            if (data_ != nullptr)
            {
                CJING_FREE_ALIGN(data_);
                data_ = nullptr;
            }
        }

        void resize(U32 size)
        {
            if (size > capacity_)
                reserve(size);
            for (U32 i = size_; i < size; i++)
                new ((char*)(data_ + i)) T;
            if (size < size_)
                DestructData(data_ + size, data_ + size_);
            size_ = size;
        }

        void reserve(U32 newCapacity)
        {
            if (newCapacity <= capacity_)
                return;
            
            if constexpr (__is_trivially_copyable(T))
            {
                size_t newSize = newCapacity * sizeof(T);
                data_ = static_cast<T*>(CJING_REMALLOC_ALIGN(data_, newCapacity * sizeof(T), alignof(T)));
            }
            else
            {
                T* newData = static_cast<T*>(CJING_MALLOC_ALIGN(newCapacity * sizeof(T), alignof(T)));
                MoveData(newData, data_, size_);
                CJING_FREE_ALIGN(data_);
                data_ = newData;
            }
            capacity_ = newCapacity;
        }

        bool empty()const {
            return size_ == 0;
        }

        U32 size() const { 
            return size_; 
        }

        U32 capacity() const { 
            return capacity_; 
        }

        T* data() { 
            return data_; 
        }

        const T* data() const {
            return data_;
        }

        const T& operator[](U32 index) const {
            ASSERT(index < size_);
            return data_[index];
        }

        T& operator[](U32 index) {
            ASSERT(index < size_);
            return data_[index];
        }

        template <typename F> 
        int find(F predicate) const 
        {
            for (U32 i = 0; i < size_; i++) 
            {
                if (predicate(data_[i]))
                    return i;
            }
            return -1;
        }

        operator Span<T>() const { return Span(begin(), end()); }
        operator Span<const T>() const { return Span(begin(), end()); }

    private:
        void MoveData(T* dst, T* src, U32 count) 
        {
            ASSERT(dst > src || dst + count < src);
            for (U32 i = count - 1; i < count; --i) 
            {
                new (dst + i) T(static_cast<T&&>(src[i]));    // Move construct
                src[i].~T();
            }
        }

        void DestructData(T* begin, T* end)
        {
            for(; begin < end; begin++)
                begin->~T();
        }

        void Grow() {
            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        }

        U32 capacity_ = 0;
        U32 size_ = 0;
        T* data_ = nullptr;
    };
}