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
    public class FileParser : ClassParser
    {
        private ParsingSettings _settings;
        private AttributeParser _attributeParser;

        private new FileParsingResult ParsingResult => GetContext.ParsingResult as FileParsingResult;

        public FileParser(ParsingSettings settings)
        {
            _settings = settings;
            _attributeParser = new AttributeParser();
        }

        public FileParser(ParsingSettings settings, AttributeParser attributeParser)
        {
            _settings = settings;
            _attributeParser = attributeParser;
        }

        public unsafe bool Parse(HeaderFile headerFile, FileParsingResult result)
        {
            string filePath = headerFile.FilePath;
            if (!File.Exists(filePath))
            {
                result.Errors.Add(new ParsingError( string.Format("File {0} doesn't exist.", filePath)));
                return false;
            }

            var translationFlags = CXTranslationUnit_Flags.CXTranslationUnit_None |
                CXTranslationUnit_Flags.CXTranslationUnit_Incomplete |
                CXTranslationUnit_Flags.CXTranslationUnit_SkipFunctionBodies |
                CXTranslationUnit_Flags.CXTranslationUnit_KeepGoing |
                CXTranslationUnit_Flags.CXTranslationUnit_DetailedPreprocessingRecord |
                CXTranslationUnit_Flags.CXTranslationUnit_IncludeAttributedTypes;

            var arguments = _settings.GetCompilationArguments();
            foreach (var includePath in headerFile.IncludePaths)
            {
                arguments.Add("-I" + includePath);
            }

            var module = headerFile.ModuleInfo;
            if (module != null)
            {
                foreach (var def in module.PublicDefines)
                {
                    arguments.Add("-D" + def);
                }
            }

            bool isSuccess = false;
            var index = CXIndex.Create();
            var translationUnitError = CXTranslationUnit.TryParse(index, filePath, arguments.ToArray(), Array.Empty<CXUnsavedFile>(), translationFlags, out var handle);
            if (translationUnitError != CXErrorCode.CXError_Success)
            {
                result.Errors.Add(new ParsingError($"Error: Parsing failed for '{filePath}' due to '{translationUnitError}'."));
            }
            else
            {
                var parsingContext = PushContext(handle, result);
                if (parsingContext != null)
                {
                    GCHandle fileParserHandle = GCHandle.Alloc(this);
                    parsingContext.RootCursor.VisitChildren(FileCursorVisitor, new CXClientData((IntPtr)fileParserHandle));
                    if (ParsingResult.Errors.Count <= 0)
                    {
                        //Refresh all outer entities contained in the final result
                        RefreshOuterEntity(ParsingResult);
                        isSuccess = true;
                    }

                    PopContext();

                    if (contextsStack.Count > 0)
                        throw new Exception("There should not have any context left once parsing has finished");
                }

                handle.Dispose();
            }

            return isSuccess;
        }

        private ParsingContext PushContext(CXTranslationUnit translationUnit, FileParsingResult fileParsingResult)
        {
            ParsingContext parsingContext = new ParsingContext();
            parsingContext.Parent = null;
            parsingContext.RootCursor = translationUnit.Cursor;
            parsingContext.ParsingResult = fileParsingResult;
            parsingContext.ParsingSettings = _settings;
            parsingContext.StructClassTree = fileParsingResult.StructClassTree;
            parsingContext.AttributeParser = _attributeParser;
            parsingContext.AttributeParser.Setup(_settings);

            contextsStack.Push(parsingContext);
            return parsingContext;
        }

        private static unsafe CXChildVisitResult FileCursorVisitor(CXCursor cursor, CXCursor parent, void* data)
        {
            CXChildVisitResult visitResult = CXChildVisitResult.CXChildVisit_Continue;

            CXClientData tmpData = (CXClientData)data;
            var tmpHandle = (GCHandle)tmpData.Handle;
            var fileParser = tmpHandle.Target as FileParser;
            if (fileParser == null)
                return CXChildVisitResult.CXChildVisit_Break;

            // Parse the given file only, ignore other included headers
            if (cursor.Location.IsFromMainFile)
            { 
                if (cursor.IsPreprocessing)
                {
                    fileParser.ParsingPreprocessing(cursor, ref visitResult);
                }
                else if (cursor.IsDeclaration)
                {
                    fileParser.ParsingDeclaration(cursor, ref visitResult);
                }
            }
            return visitResult;
        }

        private void ParsingPreprocessing(CXCursor cursor, ref CXChildVisitResult visitResult)
        {
            if (cursor.kind == CXCursorKind.CXCursor_InclusionDirective)
            {
                var includeFile = cursor.IncludedFile;
                var headerFilePath = includeFile.TryGetRealPathName().CString;
                if (headerFilePath != null && headerFilePath.Length > 0)
                    ParsingResult.IncludedHeaders.Add(headerFilePath);
            }
        }

        private void ParsingDeclaration(CXCursor cursor, ref CXChildVisitResult visitResult)
        {
            switch (cursor.Kind)
            {
                case CXCursorKind.CXCursor_Namespace:
                    ParseInNamespace(cursor, ref visitResult);
                    break;

                case CXCursorKind.CXCursor_StructDecl:
                case CXCursorKind.CXCursor_ClassDecl:
                {
                    ClassParsingResult result;
                    visitResult = Parse(cursor, GetContext, out result);
                    if (result.ParsedClass != null)
                    {
                        if (result.ParsedClass.EntityType == EntityType.Struct)
                            ParsingResult.Structs.Add(result.ParsedClass);
                        else
                            ParsingResult.Classes.Add(result.ParsedClass);
                    }
                    ParsingResult.AppendResult(result);
                }
                break;
                case CXCursorKind.CXCursor_EnumDecl:
                {
                    EnumParsingResult result;
                    visitResult = _enumParser.Parse(cursor, GetContext, out result);
                    if (result.ParsedEnum != null)
                    {
                        ParsingResult.Enums.Add(result.ParsedEnum);
                    }
                    ParsingResult.AppendResult(result);
                }
                break;
                case CXCursorKind.CXCursor_FunctionDecl:
                { 
                }
                break;
            }
        }

        public unsafe void ParseInNamespace(CXCursor cursor, ref CXChildVisitResult visitResult)
        {
            GCHandle parserHandle = GCHandle.Alloc(this);
            visitResult = cursor.VisitChildren(FileCursorVisitor, new CXClientData((IntPtr)parserHandle));
        }

        private void RefreshOuterEntity(FileParsingResult result)
        { 
        }

        public object Clone()
        {
            return new FileParser(_settings, _attributeParser);
        }
    }
}
