using ClangSharp;
using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class NestedStructClassInfo : StructClassInfo
    {
        public NestedStructClassInfo(StructClassInfo baseInfo)
        {
            baseInfo.Clone(this);
        }
    }

    public class ClassQualifiers
    {
        public bool IsFinal = false;
        public bool IsVirtual = false;
    }

    public class ClassParentInfo
    {
        public EAccessSpecifier InheritanceAccess;
        public TypeInfo Type;
	};

    public class StructClassInfo : EntityInfo
    {
        /// <summary>
        /// Class qualifiers info.
        /// </summary>
        public ClassQualifiers ClassQualifiers = new ClassQualifiers();

        /// <summary>
        /// More detailed information on this class.
        /// </summary>
        public TypeInfo? TypeInfo { get; set; } = null;

        /// <summary>
        /// List of all parent classes of this class.
        /// </summary>
        public List<ClassParentInfo> Parent = new List<ClassParentInfo>();

        /// <summary>
        /// List of all nested classes contained in this class.
        /// </summary>
        public List<NestedStructClassInfo> NestedClasses = new List<NestedStructClassInfo>();

        /// <summary>
        /// List of all nested structs contained in this class.
        /// </summary>
        public List<NestedStructClassInfo> NestedStructs = new List<NestedStructClassInfo>();

        /// <summary>
        /// List of all enums contained in this class.
        /// </summary>
        public List<EnumInfo> NestedEnums = new List<EnumInfo>();

        /// <summary>
        /// List of all fields contained in this class.
        /// </summary>
        public List<FieldInfo> Fields = new List<FieldInfo>();

        /// <summary>
        /// List of all methods contained in this class.
        /// </summary>
        public List<MethodInfo> Methods = new List<MethodInfo>();

        public bool IsForwardDecl { get; } = false;

        public StructClassInfo() { }
        public StructClassInfo(CXCursor cursor, List<AttributeInfo> attributes, bool isForwardDecl) :
            base(cursor, attributes, (cursor.Kind == CXCursorKind.CXCursor_StructDecl) ? EntityType.Struct : EntityType.Class)
        {
            TypeInfo = new TypeInfo(cursor);
            IsForwardDecl = isForwardDecl;
        }

        public void RefreshOuterEntity()
        {
            foreach (var nestedClass in NestedClasses)
            {
                nestedClass.RefreshOuterEntity();
                nestedClass.Outer = this;
            }

            foreach (var nestedClass in NestedStructs)
            {
                nestedClass.RefreshOuterEntity();
                nestedClass.Outer = this;
            }

            foreach (var nestedClass in NestedEnums)
            {
                nestedClass.RefreshOuterEntity();
                nestedClass.Outer = this;
            }

            foreach (var field in Fields)
            {
                field.Outer = this;
            }

            foreach (var method in Methods)
            {
                method.Outer = this;
            }
        }

        public void Clone(StructClassInfo otherClass)
        { 
            base.Clone(otherClass);

            otherClass.Parent.AddRange(Parent);
            otherClass.NestedClasses.AddRange(NestedClasses);
            otherClass.NestedStructs.AddRange(NestedStructs);
            otherClass.Fields.AddRange(Fields);
            otherClass.Methods.AddRange(Methods);
            
            if (TypeInfo != null)
                otherClass.TypeInfo = TypeInfo.Clone();
        }
    }
}
