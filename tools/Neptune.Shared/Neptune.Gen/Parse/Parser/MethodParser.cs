using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class MethodParser : EntityParser<MethodParser, MethodParsingResult>
    {
        protected override bool ShouldParseCurrentEntity(CXCursor cursor)
        {
            return GetContext.ParsingSettings.ShouldParseAllMethods;
        }

        protected override void SetParsedEntity(CXCursor cursor, List<AttributeInfo> attributeInfos)
        {
            ParsingResult.ParsedMethod = new MethodInfo(GetContext.RootCursor, attributeInfos);
        }

        protected override bool ShouldSkipPreAttributes(CXCursor cursor)
        {
            // Skip DLLImport/DLLExport
            if (cursor.kind == CXCursorKind.CXCursor_DLLImport ||
                cursor.kind == CXCursorKind.CXCursor_DLLExport)
            {
                return true;
            }
            return false;
        }

        protected override CXChildVisitResult ParseNestedEntity(CXCursor cursor)
        {
            var parsedMethod = ParsingResult.ParsedMethod;
            if (parsedMethod != null)
            {
                switch (cursor.Kind)
                {
                    case CXCursorKind.CXCursor_CXXFinalAttr:
                        parsedMethod.IsFinal = true;
                        break;

                    case CXCursorKind.CXCursor_CXXOverrideAttr:
                        parsedMethod.IsOverride = true;
                        break;

                    case CXCursorKind.CXCursor_ParmDecl:
                        parsedMethod.Parameters.Add(new FunctionParamInfo(cursor.DisplayName.CString, new TypeInfo(cursor.Type)));
                        break;
                }
            }

            return CXChildVisitResult.CXChildVisit_Recurse;
        }

        protected override List<AttributeInfo>? GetCurrentEntityAttribute(CXCursor cursor)
        {
            return GetContext.AttributeParser.GetMethodAttributeInfos(cursor.Spelling.CString);
        }
    }
}
