using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace Neptune.Gen
{
    public struct FunctionParamInfo
    {
        public string Name;
        public TypeInfo Type;

        public FunctionParamInfo(string name, TypeInfo type)
        {
            Name = name;
            Type = type;
        }
    }

    public class FunctionInfo : EntityInfo
    {
        public string Prototype;
        public TypeInfo ReturnType;
        public bool IsStatic = false;
        public bool IsInline = false;
        public List<FunctionParamInfo> Parameters = new List<FunctionParamInfo>();

        public FunctionInfo(CXCursor cursor, List<AttributeInfo> attributes) :
            this(cursor, attributes, EntityType.Function)
        {
            IsStatic = cursor.Linkage == CXLinkageKind.CXLinkage_Internal;         
        }

        public FunctionInfo(CXCursor cursor, List<AttributeInfo> attributes, EntityType entityType) :
            base(cursor, attributes, entityType)
        {
            CXType functionType = cursor.Type;
            if (functionType.kind != CXTypeKind.CXType_FunctionProto)
                throw new InvalidOperationException();

            Prototype = functionType.Spelling.CString;
            ReturnType = new TypeInfo(functionType.ResultType);
            Name = Name.Substring(0, Name.IndexOf('('));
            IsStatic = false;
            IsInline = cursor.IsFunctionInlined;
        }
    }
}
