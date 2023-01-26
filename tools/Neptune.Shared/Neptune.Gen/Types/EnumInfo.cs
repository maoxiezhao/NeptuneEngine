using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public sealed class EnumValueInfo : EntityInfo
    {
        public long EnumValue = 0;

        public EnumValueInfo(CXCursor cursor, List<AttributeInfo> attributes) :
            base(cursor, attributes, EntityType.EnumValue)
        {
            EnumValue = cursor.EnumConstantDeclValue;
        }
    }

    public class EnumInfo : EntityInfo
    {
        public TypeInfo? Type { get; }
        public TypeInfo? UnderlyingType { get; }

        public List<EnumValueInfo> EnumValues = new List<EnumValueInfo>();

        public EnumInfo(CXCursor cursor, List<AttributeInfo> attributes) :
            base(cursor, attributes, EntityType.Enum)
        {
            Type = new TypeInfo(cursor.Type);
            UnderlyingType = new TypeInfo(cursor.EnumDecl_IntegerType);
        }
    }
}
