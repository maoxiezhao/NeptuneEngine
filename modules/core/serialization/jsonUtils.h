#pragma once

#include "core\common.h"
#include "core\scene\world.h"
#include "json.h"

#include "core\types\guid.h"
#include "core\utils\string.h"

namespace VulkanTest
{
	class VULKAN_TEST_API JsonUtils
	{
	public:
		using Document = rapidjson_flax::Document ;
		using Value = rapidjson_flax::Value ;

        static Guid GetGuid(const Value& value);
        static ECS::Entity GetEntity(World* world, const Value& value);

        FORCE_INLINE static bool GetBool(const Value& node, const char* name, const bool defaultValue)
        {
            auto member = node.FindMember(name);
            return member != node.MemberEnd() ? member->value.GetBool() : defaultValue;
        }

        FORCE_INLINE static F32 GetFloat(const Value& node, const char* name, const float defaultValue)
        {
            auto member = node.FindMember(name);
            return member != node.MemberEnd() && member->value.IsNumber() ? member->value.GetFloat() : defaultValue;
        }

        FORCE_INLINE static I32 GetInt(const Value& node, const char* name, const I32 defaultValue)
        {
            auto member = node.FindMember(name);
            return member != node.MemberEnd() && member->value.IsInt() ? member->value.GetInt() : defaultValue;
        }

        FORCE_INLINE static U32 GetUint(const Value& node, const char* name, const U32 defaultValue)
        {
            auto member = node.FindMember(name);
            return member != node.MemberEnd() && member->value.IsUint() ? member->value.GetUint() : defaultValue;
        }

        template<class T>
        FORCE_INLINE static T GetEnum(const Value& node, const char* name, const T defaultValue)
        {
            auto member = node.FindMember(name);
            return member != node.MemberEnd() && member->value.IsInt() ? static_cast<T>(member->value.GetInt()) : defaultValue;
        }

        FORCE_INLINE static String GetString(const Value& node, const char* name)
		{
			auto member = node.FindMember(name);
			return member != node.MemberEnd() ? String(member->value.GetString()) : String::EMPTY;
		}

        FORCE_INLINE static Guid GetGuid(const Value& node, const char* name)
        {
            auto member = node.FindMember(name);
            return member != node.MemberEnd() ? GetGuid(member->value) : Guid::Empty;
        }

        FORCE_INLINE static ECS::Entity GetEntity(const Value& node, const char* name, World* world)
        {
            auto member = node.FindMember(name);
            return member != node.MemberEnd() ? GetEntity(world, member->value) : ECS::INVALID_ENTITY;
        }
	};
}