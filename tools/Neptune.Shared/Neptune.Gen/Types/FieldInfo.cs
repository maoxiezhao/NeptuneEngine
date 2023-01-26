using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class FieldInfo : VariableInfo
    {
        public EAccessSpecifier AccessSpecifier = EAccessSpecifier.Invalid;
        public bool IsMutable { get; } = false;
        public Int64 MemoryOffset = 0;

        public FieldInfo(CXCursor cursor, List<AttributeInfo> attributes) :
            base(cursor, attributes, EntityType.Field)
        {
            IsMutable = cursor.CXXField_IsMutable;

            // TODO
            IsStatic = cursor.Kind == CXCursorKind.CXCursor_VarDecl;

            if (!IsStatic)
            {
                MemoryOffset = cursor.OffsetOfField;
                MemoryOffset /= 8; // Convert to bytes
            }
        }
    }
}
