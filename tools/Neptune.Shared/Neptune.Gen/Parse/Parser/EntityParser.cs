using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public abstract class EntityParserBase
    {
        protected Stack<ParsingContext> contextsStack = new Stack<ParsingContext>();

        protected ParsingContext GetContext => contextsStack.Peek();

        protected virtual ParsingContext PushContext(CXCursor cursor, ParsingContext parent, ParsingResultBase parsingResult)
        {
            ParsingContext parsingContext = new ParsingContext();
            parsingContext.Parent = parent;
            parsingContext.RootCursor = cursor;
            parsingContext.ParsingResult = parsingResult;
            parsingContext.ParsingSettings = parent.ParsingSettings;
            parsingContext.AttributeParser = parent.AttributeParser;

            contextsStack.Push(parsingContext);
            return parsingContext;
        }

        protected virtual void PopContext() 
        {
            contextsStack.Pop();
        }
    }

    public class EntityParser<ParserT, ResultT> : EntityParserBase where ParserT : class where ResultT : ParsingResultBase, new()
    {
        private bool _shouldCheckAttributes = true;
        protected ResultT ParsingResult => GetContext.ParsingResult as ResultT;

        public unsafe CXChildVisitResult Parse(CXCursor cursor, ParsingContext parentContext, out ResultT parsingResult)
        {
            parsingResult = new ResultT();

            if (parentContext.ParsingSettings == null)
                return CXChildVisitResult.CXChildVisit_Break;

            var parsingContext = PushContext(cursor, parentContext, parsingResult);
            if (parsingContext != null)
            {
                GCHandle parserHandle = GCHandle.Alloc(this);
                cursor.VisitChildren(CursorVisitor, new CXClientData((IntPtr)parserHandle));
                PopContext();
            }

            if (parentContext.ParsingSettings.ShouldAbortParsingOnFirstError && parsingResult.Errors.Count > 0)
                return CXChildVisitResult.CXChildVisit_Break;

            return CXChildVisitResult.CXChildVisit_Continue;
        }

        private static unsafe CXChildVisitResult CursorVisitor(CXCursor cursor, CXCursor parent, void* data)
        {
            CXClientData tmpData = (CXClientData)data;
            var tmpHandle = (GCHandle)tmpData.Handle;
            var parser = tmpHandle.Target as EntityParser<ParserT, ResultT>;
            if (parser == null)
                return CXChildVisitResult.CXChildVisit_Break;

            // Check attributes
            if (parser._shouldCheckAttributes)
            {
                if (parser.ShouldSkipPreAttributes(cursor))
                    return CXChildVisitResult.CXChildVisit_Continue;

                parser._shouldCheckAttributes = false;
                return parser.CheckAndSetParsedEntity(cursor);
            }

            return parser.ParseNestedEntity(cursor);
        }

        private CXChildVisitResult CheckAndSetParsedEntity(CXCursor cursor)
        {
            ParsingContext context = GetContext;
            if (cursor.kind != CXCursorKind.CXCursor_AnnotateAttr)
            {
                if (ShouldParseCurrentEntity(cursor))
                {
                    SetParsedEntity(context.RootCursor, new List<AttributeInfo>());
                    return CXChildVisitResult.CXChildVisit_Break;
                }
                else
                {
                    return CXChildVisitResult.CXChildVisit_Break;
                }
            }

            if (context.AttributeParser == null)
                return CXChildVisitResult.CXChildVisit_Break;

            List<AttributeInfo>? attributeInfos = GetCurrentEntityAttribute(cursor);
            if (attributeInfos != null)
            {
                SetParsedEntity(context.RootCursor, attributeInfos);
                return CXChildVisitResult.CXChildVisit_Recurse;
            }
            else
            {
                if (context.AttributeParser.LastErrorMsg.Length > 0)
                    context.ParsingResult.Errors.Add(new ParsingError(context.AttributeParser.LastErrorMsg));

                return CXChildVisitResult.CXChildVisit_Break;
            }
        }

        protected virtual bool ShouldParseCurrentEntity(CXCursor cursor)
        {
            return false;
        }

        protected virtual List<AttributeInfo>? GetCurrentEntityAttribute(CXCursor cursor)
        {
            return null;
        }

        protected virtual void SetParsedEntity(CXCursor cursor, List<AttributeInfo> attributeInfos)
        { 
        }

        protected virtual bool ShouldSkipPreAttributes(CXCursor cursor)
        {
            return false;   
        }

        protected virtual CXChildVisitResult ParseNestedEntity(CXCursor cursor)
        {
            return CXChildVisitResult.CXChildVisit_Continue;
        }
    }
}