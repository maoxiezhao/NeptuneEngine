#pragma once

#include "core\common.h"
#include "iSerializable.h"
#include "jsonWriter.h"
#include "json.h"

namespace VulkanTest
{
#define SERIALIZE_EPSILON 1e-7f
#define SERIALIZE_EPSILON_DOUBLE 1e-17

#define SERIALIZE_GET_OTHER_OBJ(type) const auto other = static_cast<const type*>(otherObj)
#define SERIALIZE_FIND_MEMBER(stream, name) stream.FindMember(rapidjson_flax::Value(name, ARRAYSIZE(name) - 1))

#define SERIALIZE(name) \
    if (Serialization::ShouldSerialize(name, other ? &other->name : nullptr)) \
	{ \
		stream.JKEY(#name); \
		Serialization::Serialize(stream, name, other ? &other->name : nullptr); \
	}

#define SERIALIZE_MEMBER(name, member) \
    if (Serialization::ShouldSerialize(member, other ? &other->member : nullptr)) \
	{ \
		stream.JKEY(#name); \
		Serialization::Serialize(stream, member, other ? &other->member : nullptr); \
	}

#define SERIALIZE_OBJECT_MEMBER(name, obj, member) \
    if (Serialization::ShouldSerialize(obj.member, other ? &other->member : nullptr)) \
	{ \
		stream.JKEY(#name); \
		Serialization::Serialize(stream, obj.member, other ? &other->member : nullptr); \
	}

#define DESERIALIZE(name)  \
    { \
        const auto e = SERIALIZE_FIND_MEMBER(stream, #name); \
        if (e != stream.MemberEnd()) \
            Serialization::Deserialize(e->value, name); \
    }

#define DESERIALIZE_MEMBER(name, member) \
    { \
        const auto e = SERIALIZE_FIND_MEMBER(stream, #name); \
        if (e != stream.MemberEnd()) \
            Serialization::Deserialize(e->value, member); \
    }
}