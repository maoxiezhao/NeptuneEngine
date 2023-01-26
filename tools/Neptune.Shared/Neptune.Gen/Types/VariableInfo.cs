using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class VariableInfo : EntityInfo
    {
        public bool IsStatic = false;
        public TypeInfo? Type { get; }

        public VariableInfo(CXCursor cursor, List<AttributeInfo> attributes) :
            this(cursor, attributes, EntityType.Variable)
        {
        }

        public VariableInfo(CXCursor cursor, List<AttributeInfo> attributes, EntityType entityType) :
            base(cursor, attributes, entityType)
        {
            IsStatic = false;
            Type = new TypeInfo(cursor);
        }
    }
}
