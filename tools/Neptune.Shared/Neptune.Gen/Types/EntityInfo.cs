using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public enum EntityType
    {
        Undefined = 0,
        Class = 1 << 0,
        Struct = 1 << 1,
        Enum = 1 << 2,
        Variable = 1 << 3,
        Field = 1 << 4,
        Function = 1 << 5,
        Method = 1 << 6,
        EnumValue = 1 << 7,
    }

    public class EntityInfo
    {
        public EntityType EntityType = EntityType.Undefined;
        public string Name = string.Empty;
        public string ID = string.Empty;
        public EntityInfo Outer = null;
        public List<AttributeInfo> Attributes = new List<AttributeInfo>();

        public string FullName => Outer != null ? Outer.FullName + "::" + Name : Name;

        public EntityInfo() { }
        public EntityInfo(CXCursor cursor, List<AttributeInfo> attributes, EntityType entityType)
        { 
            EntityType = entityType;
            Name = cursor.DisplayName.CString;
            ID = cursor.Usr.CString;
            Attributes = attributes;
        }

        public static string GetFullName(CXCursor cursor)
        {
            var parentCursor = cursor.LexicalParent;
            if (parentCursor.IsNull || parentCursor.Kind == CXCursorKind.CXCursor_TranslationUnit)
                return cursor.DisplayName.CString;

            return parentCursor.DisplayName + "::" + cursor.Name;
        }

        public void Clone(EntityInfo other)
        {
            other.EntityType = EntityType;
            other.Name = Name;
            other.ID = ID;
            other.Attributes = Attributes;
        }
    }
}
