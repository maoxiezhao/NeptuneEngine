using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class EnumValueParser : EntityParser<EnumValueParser, EnumValueParsingResult>
    {
        protected override bool ShouldParseCurrentEntity(CXCursor cursor)
        {
            return GetContext.ParsingSettings.ShouldParseAllEnumValues;
        }

        protected override void SetParsedEntity(CXCursor cursor, List<AttributeInfo> attributeInfos)
        {
            ParsingResult.ParsedEnumValue = new EnumValueInfo(GetContext.RootCursor, attributeInfos);
        }

        protected override CXChildVisitResult ParseNestedEntity(CXCursor cursor)
        {
            return CXChildVisitResult.CXChildVisit_Break;
        }

        protected override List<AttributeInfo>? GetCurrentEntityAttribute(CXCursor cursor)
        {
            return GetContext.AttributeParser.GetEnumValueAttributeInfos(cursor.Spelling.CString);
        }
    }

    public class EnumParser : EntityParser<EnumParser, EnumParsingResult>
    {
        private EnumValueParser _valueParser = new EnumValueParser();

        protected override bool ShouldParseCurrentEntity(CXCursor cursor)
        {
            return GetContext.ParsingSettings.ShouldParseAllEnums;
        }

        protected override void SetParsedEntity(CXCursor cursor, List<AttributeInfo> attributeInfos)
        {
            ParsingResult.ParsedEnum = new EnumInfo(GetContext.RootCursor, attributeInfos);
        }

        protected override CXChildVisitResult ParseNestedEntity(CXCursor cursor)
        {
            var visitResult = CXChildVisitResult.CXChildVisit_Continue;
            var parsedEnum = ParsingResult.ParsedEnum;
            if (parsedEnum != null)
            {
                switch (cursor.Kind)
                {
                    case CXCursorKind.CXCursor_EnumConstantDecl:
                    {
                        EnumValueParsingResult result;
                        visitResult = _valueParser.Parse(cursor, GetContext, out result);
                        if (result.ParsedEnumValue != null)
                        {
                            parsedEnum.EnumValues.Add(result.ParsedEnumValue);
                        }
                        ParsingResult.AppendResult(result);
                    }
                    break;
                }
            }

            return visitResult;
        }

        protected override List<AttributeInfo>? GetCurrentEntityAttribute(CXCursor cursor)
        {
            return GetContext.AttributeParser.GetEnumAttributeInfos(cursor.Spelling.CString);
        }
    }
}
