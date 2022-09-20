#pragma once

#include "core\common.h"
#include "core\collections\hashMap.h"
#include "core\platform\platform.h"

namespace VulkanTest {

	struct VULKAN_TEST_API Guid
	{
	public:
        union
        {
            struct
            {
                U32 A;
                U32 B;
                U32 C;
                U32 D;
            };

            U8 Raw[16];
            U32 Values[4];
        };

    public:
        Guid()
        {
        }

        Guid(U32 a, U32 b, U32 c, U32 d)
            : A(a)
            , B(b)
            , C(c)
            , D(d)
        {
        }

        FORCE_INLINE bool IsValid() const
        {
            return (A | B | C | D) != 0;
        }

        explicit operator bool() const
        {
            return (A | B | C | D) != 0;
        }

        friend bool operator==(const Guid& left, const Guid& right)
        {
            return ((left.A ^ right.A) | (left.B ^ right.B) | (left.C ^ right.C) | (left.D ^ right.D)) == 0;
        }

        friend bool operator!=(const Guid& left, const Guid& right)
        {
            return ((left.A ^ right.A) | (left.B ^ right.B) | (left.C ^ right.C) | (left.D ^ right.D)) != 0;
        }

        inline U64 GetHash()const {
            return A ^ B ^ C ^ D;
        }

        FORCE_INLINE static Guid New()
        {
            Guid result;
            Platform::CreateGuid(&result);
            return result;
        }

        static Guid Empty;
	};

    template<>
    struct HashMapHashFunc<Guid>
    {
        static U32 Get(const Guid& key)
        {
            return (U32)key.GetHash();
        }
    };
}