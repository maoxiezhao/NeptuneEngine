#pragma once

#include <list>
#include <vector>
#include <unordered_map>

#include "math\hash.h"
#include "objectPool.h"

namespace VulkanTest
{
    namespace Util
    {
        template <typename T>
        class TempHashMapItem
        {
        public:
            void SetHash(HashValue hash_)
            {
                hash = hash_;
            }
            HashValue GetHash()
            {
                return hash;
            }

            void SetIndex(unsigned index_)
            {
                index = index_;
            }
            unsigned GetIndex() const
            {
                return index;
            }

        private:
            HashValue hash = 0;
            unsigned index = 0;
        };

        template <typename T, size_t RingSize, bool Reuse = false>
        class TempHashMap
        {
        private:
            U32 ringIndex = 0;
            ObjectPool<T> objectPool;
            std::unordered_map<HashValue, T *> hashMap;
            std::vector<T *> vacants;
            std::list<T *> rings[RingSize];

            template <bool reuse>
            struct ReuseTag
            {
            };

            void FreeObject(T *object, const ReuseTag<false> &)
            {
                objectPool.free(object);
            }

            void FreeObject(T *object, const ReuseTag<true> &)
            {
                vacants.push_back(object);
            }

        public:
            ~TempHashMap()
            {
                Clear();
            }

            void Clear()
            {
                for (std::list<T *> &list : rings)
                {
                    for (auto &obj : list)
                        objectPool.free(static_cast<T *>(obj));
                    list.clear();
                }

                for (auto &vacant : vacants)
                    objectPool.free(static_cast<T *>(vacant));
                vacants.clear();

                objectPool.clear();
                hashMap.clear();
            }

            void BeginFrame()
            {
                ringIndex = (ringIndex + 1) % (RingSize - 1);
                for (auto node : rings[ringIndex])
                {
                    hashMap.erase(node->GetHash());
                    FreeObject(node, ReuseTag<Reuse>());
                }
                rings[ringIndex].clear();
            }

            template <typename... Args>
            void MakeVacant(Args &&...args)
            {
                auto obj = objectPool.allocate(std::forward<Args>(args)...);
                vacants.push_back(obj);
            }

            T *Requset(HashValue hash)
            {
                auto it = hashMap.find(hash);
                if (it == hashMap.end())
                    return nullptr;

                T *ret = it->second;
                if (ret->GetIndex() != ringIndex)
                {
                    auto it = std::find(rings[ret->GetIndex()].begin(), rings[ret->GetIndex()].end(), ret);
                    if (it != rings[ret->GetIndex()].end())
                    {
                        rings[ret->GetIndex()].erase(it);
                        rings[ringIndex].push_back(ret);
                        ret->SetIndex(ringIndex);
                    }
                }

                return ret;
            }

            T *RequestVacant(HashValue hash)
            {
                if (vacants.empty())
                    return nullptr;

                auto top = vacants.back();
                vacants.pop_back();
                top->SetIndex(ringIndex);
                top->SetHash(hash);
                hashMap.insert(std::make_pair(hash, top));
                rings[ringIndex].push_back(top);
                return top;
            }

            template <typename... Args>
            T *Emplace(HashValue hash, Args &&...args)
            {
                auto obj = objectPool.allocate(std::forward<Args>(args)...);
                obj->SetHash(hash);
                obj->SetIndex(ringIndex);
                hashMap[hash] = obj;
                rings[ringIndex].push_back(obj);
                return obj;
            }
        };
    }
}