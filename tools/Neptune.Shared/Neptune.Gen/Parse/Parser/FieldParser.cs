using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class FieldParser : EntityParser<FieldParser, FieldParsingResult>
    {
        protected override bool ShouldParseCurrentEntity(CXCursor cursor)
        {
            return GetContext.ParsingSettings.ShouldParseAllFields;
        }

        protected override void SetParsedEntity(CXCursor cursor, List<AttributeInfo> attributeInfos)
        {
            ParsingResult.ParsedField = new FieldInfo(GetContext.RootCursor, attributeInfos);
        }

        protected override CXChildVisitResult ParseNestedEntity(CXCursor cursor)
        {
            return CXChildVisitResult.CXChildVisit_Continue;
        }

        protected override List<AttributeInfo>? GetCurrentEntityAttribute(CXCursor cursor)
        {
            return GetContext.AttributeParser.GetFieldAttributeInfos(cursor.Spelling.CString);
        }
    }
}
