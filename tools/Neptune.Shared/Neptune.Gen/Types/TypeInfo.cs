using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public enum ETypeDescriptor
    {
        Undefined,

        Const = 1 << 0,
        Restrict = 1 << 1,
        Volatile = 1 << 2,

        Ptr = 1 << 3,
        LRef = 1 << 4,
        RRef = 1 << 5,
        CArray = 1 << 6,
        Value = 1 << 7	//Means non-ptr simple value, ex: int
    }

    public class TypePart
    {
        /// <summary>
        /// Padding to align memory.
        /// </summary>
        public UInt16 Padding;

        /// <summary>
        /// Descriptor of this part of the type.
        /// </summary>
        public ETypeDescriptor Descriptor;

        /// <summary>
        /// If descriptor is a CArray, additionalData is size. Might contain other information in other cases.
        /// </summary>
        public UInt32 AdditionalData;

        public TypePart(UInt16 padding, ETypeDescriptor descriptor, UInt32 additionData)
        {
            Padding = padding;
            Descriptor = descriptor;
            AdditionalData = additionData;
        }
    };

    public class TypeInfo
    {
        /// <summary>
        /// The full name represents the type name, containing all its qualifiers.
        /// </summary>
        public string FullName => _fullName;
        public string CanonicalName => _canonicalName;
        public List<TypePart> TypeParts = new List<TypePart>();
        public List<TemplateParamInfo> TemplateParamInfos = new List<TemplateParamInfo>();
        public bool IsTemplateType => TemplateParamInfos.Count > 0;

        private string _fullName = string.Empty;
        private string _canonicalName = string.Empty;
        private uint _sizeInBytes = 0;

        public TypeInfo()
        { 
        }

        public TypeInfo(CXType type)
        {
            Initialzie(type);
        }

        public TypeInfo(CXCursor cursor)
        {
            switch (cursor.Kind)
            {
                case CXCursorKind.CXCursor_ClassTemplate:
                    _fullName = ComputeClassTemplateFullname(cursor);
                    _canonicalName = FullName;
                    FillTemplateParameters(cursor);
                    break;
                case CXCursorKind.CXCursor_TemplateTemplateParameter:
                    _fullName = cursor.Spelling.CString;
                    _canonicalName = FullName;
                    FillTemplateParameters(cursor);
                    break;

                case CXCursorKind.CXCursor_TemplateTypeParameter:
                    Initialzie(cursor.Type);
                    break;

                default:
                    CXType cursorType = cursor.Type;
                    if (cursorType.kind == CXTypeKind.CXType_Invalid)
                        throw new Exception("Invalidi cursor type " + cursor.DisplayName);

                    // Template type
                    if (cursorType.SizeOf == (long)CXTypeLayoutError.CXTypeLayoutError_Dependent &&
                        IsTemplateTypename(cursorType.Spelling.CString))
                    {
                        FillTemplateParameters(cursor);
                    }

                    Initialzie(cursorType);
                    break;
            }
        }

        private void Initialzie(CXType type)
        {
            var canonicalType = type.CanonicalType;
            if (canonicalType.kind == CXTypeKind.CXType_Invalid)
                throw new Exception("Invalidi canonical type " + type.TypedefName);

            _fullName = type.Spelling.CString;
            _canonicalName = canonicalType.Spelling.CString;

            // Get size of type
            long size = type.SizeOf;
            if (size == (long)CXTypeLayoutError.CXTypeLayoutError_Invalid ||
                size == (long)CXTypeLayoutError.CXTypeLayoutError_Incomplete ||
                size == (long)CXTypeLayoutError.CXTypeLayoutError_Dependent)
            {
                _sizeInBytes = 0;
            }
            else
            {
                _sizeInBytes = (uint)size;
            }

            // Remove class or struct keyword
            if (_fullName.Length > 7)
            {
                string expectedKeyword = _fullName.Substring(0, 7);
                if (expectedKeyword == "struct ")
                {
                    _fullName = _fullName.Substring(8);
                }
                else
                {
                    expectedKeyword = _fullName.Substring(0, 6);
                    if (expectedKeyword == "class ")
                    {
                        _fullName = _fullName.Substring(7);
                    }
                }
            }

            // Fill the descriptors vector
            TypePart curTypePart = null;
            CXType curType = canonicalType;
            CXType prevType = curType;
            bool reachedValue = false;
            while (!reachedValue)
            {
                if (curType.kind == CXTypeKind.CXType_Pointer)
                {
                    curTypePart = new TypePart(0, ETypeDescriptor.Ptr, 0);
                    prevType = curType;
                    curType = prevType.PointeeType;
                }
                else if (curType.kind == CXTypeKind.CXType_LValueReference)
                {
                    curTypePart = new TypePart(0, ETypeDescriptor.LRef, 0);
                    prevType = curType;
                    curType = prevType.PointeeType;
                }
                else if (curType.kind == CXTypeKind.CXType_RValueReference)
                {
                    curTypePart = new TypePart(0, ETypeDescriptor.RRef, 0);
                    prevType = curType;
                    curType = prevType.PointeeType;
                }
                else if (curType.kind == CXTypeKind.CXType_ConstantArray)
                {
                    curTypePart = new TypePart(0, ETypeDescriptor.CArray, (uint)curType.ArraySize);
                    prevType = curType;
                    curType = prevType.ArrayElementType;
                }
                else
                {
                    curTypePart = new TypePart(0, ETypeDescriptor.Value, 0);
                    reachedValue = true;
                }

                if (curType.IsConstQualified)
                    curTypePart.Descriptor |= ETypeDescriptor.Const;
                if (curType.IsVolatileQualified)
                    curTypePart.Descriptor |= ETypeDescriptor.Volatile;
                if (curType.IsRestrictQualified)
                    curTypePart.Descriptor |= ETypeDescriptor.Restrict;

                TypeParts.Add(curTypePart);
            }
        }

        private string ComputeClassTemplateFullname(CXCursor cursor)
        {
            string fullname = cursor.Spelling.CString;
            CXCursor last = cursor;
            CXCursor current = last.SemanticParent;
            while (current != last && current.Kind != CXCursorKind.CXCursor_TranslationUnit)
            {
                fullname = current.Spelling.CString + "::" + fullname;
                last = current;
                current = cursor.SemanticParent;
            }
            return fullname;
        }

        private void FillTemplateParameters(CXCursor cursor)
        { 
        }

        private static bool IsTemplateTypename(string name)
        {
            return name.Contains('<');
        }

        public string GetName(bool removeQualifiers = false,
                              bool shouldRemoveNamespacesAndNestedClasses = false,
                              bool removeTemplateParameters = false)
        {
            string fullName = FullName;
            if (removeQualifiers)
            {
                RemoveConstQualifier(ref fullName);
                RemoveVolatileQualifier(ref fullName);
                RemoveRestrictQualifier(ref fullName);
            }

            if (shouldRemoveNamespacesAndNestedClasses)
            {
                // TODO
                throw new NotImplementedException();
            }

            if (removeTemplateParameters)
            {
                // TODO
                throw new NotImplementedException();
            }

            return fullName;
        }

        private bool RemoveConstQualifier(ref string typeString)
        {
            if (TypeParts.Count == 0 || ((int)TypeParts.Last().Descriptor & (int)ETypeDescriptor.Const) == 0)
                return false;

            var idx = typeString.LastIndexOf("const");
            if (idx == -1)
                return false;

            typeString = typeString.Remove(idx, 5);
            return true;
        }

        private bool RemoveVolatileQualifier(ref string typeString)
        {
            if (TypeParts.Count == 0 || ((int)TypeParts.Last().Descriptor & (int)ETypeDescriptor.Volatile) == 0)
                return false;

            var idx = typeString.LastIndexOf("volatile");
            if (idx == -1)
                return false;

            typeString = typeString.Remove(idx, 8);
            return true;
        }

        private bool RemoveRestrictQualifier(ref string typeString)
        {
            if (TypeParts.Count == 0 || ((int)TypeParts.Last().Descriptor & (int)ETypeDescriptor.Volatile) == 0)
                return false;

            var idx = typeString.LastIndexOf("restrict");
            if (idx == -1)
                return false;

            typeString = typeString.Remove(idx, 8);
            return true;
        }

        public TypeInfo Clone()
        {
            var newOne = new TypeInfo();
            newOne._fullName = _fullName;
            newOne._canonicalName = _canonicalName;
            newOne._sizeInBytes = _sizeInBytes;
            newOne.TypeParts.AddRange(TypeParts);
            return newOne;
        }
    }
}
