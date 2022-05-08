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
            DestructData(data_, data_ + size_);
            CJING_FREE_ALIGN(data_);
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
                if (index != size_ - 1)
                {
                    // Move the last item to the current item
                    new (NewPlaceHolder(), data_ + index) T(static_cast<T&&>(data_[size_ - 1])); 
                    data_[size_ - 1].~T();
                }
                else
                {
                    data_[index].~T();
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

        const T& operator[](U32 index) const {
            ASSERT(index < size_);
            return data_[index];
        }

        T& operator[](U32 index) {
            ASSERT(index < size_);
            return data_[index];
        }

    private:
        void MoveData(T* dst, T* src, U32 count) 
        {
            ASSERT(dst > src || dst + count < src);
            for (U32 i = count - 1; i < count; --i) 
            {
                new (NewPlaceHolder(), dst + i) T(static_cast<T&&>(src[i]));    // Move construct
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