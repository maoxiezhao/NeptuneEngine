using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class MethodInfo : FunctionInfo
    {
        public EAccessSpecifier AccessSpecifier = EAccessSpecifier.Invalid;
        public bool IsVirtual = false;
        public bool IsPureVirtual = false;
        public bool IsOverride = false;
        public bool IsFinal = false;
        public bool IsConst = false;
        public bool IsDefault = false;

        public MethodInfo(CXCursor cursor, List<AttributeInfo> attributes) :
            base(cursor, attributes, EntityType.Field)
        {
            IsDefault = cursor.CXXMethod_IsDefaulted;
            IsVirtual = cursor.CXXMethod_IsVirtual;
            IsPureVirtual = cursor.CXXMethod_IsPureVirtual;
            IsOverride = false;
            IsFinal = false;
            IsConst = cursor.CXXMethod_IsConst;
            IsStatic = cursor.CXXMethod_IsStatic;
        }
    }
}
