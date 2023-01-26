using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class FunctionParser : EntityParser<FunctionParser, FunctionParsingResult>
    {
        protected override bool ShouldParseCurrentEntity(CXCursor cursor)
        {
            return GetContext.ParsingSettings.ShouldParseAllFunctions;
        }

        protected override void SetParsedEntity(CXCursor cursor, List<AttributeInfo> attributeInfos)
        {
            ParsingResult.FunctionInfo = new FunctionInfo(GetContext.RootCursor, attributeInfos);
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
            var FunctionInfo = ParsingResult.FunctionInfo;
            if (FunctionInfo != null)
            {
                switch (cursor.Kind)
                {
                    case CXCursorKind.CXCursor_ParmDecl:
                        FunctionInfo.Parameters.Add(new FunctionParamInfo(cursor.DisplayName.CString, new TypeInfo(cursor.Type)));
                        break;
                }
            }

            return CXChildVisitResult.CXChildVisit_Recurse;
        }

        protected override List<AttributeInfo>? GetCurrentEntityAttribute(CXCursor cursor)
        {
            return GetContext.AttributeParser.GetFunctionAttributeInfos(cursor.Spelling.CString);
        }
    }
}
