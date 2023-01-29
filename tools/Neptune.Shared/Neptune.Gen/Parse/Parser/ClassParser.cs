using ClangSharp;
using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Metadata;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class ClassParser : EntityParser<ClassParser, ClassParsingResult>
    {
        protected EnumParser _enumParser = new EnumParser();
        protected FieldParser _fieldParser = new FieldParser();
        protected MethodParser _methodParser = new MethodParser();

        protected override ParsingContext PushContext(CXCursor cursor, ParsingContext parent, ParsingResultBase parsingResult)
        {
            ParsingContext parsingContext = new ParsingContext();
            parsingContext.Parent = parent;
            parsingContext.RootCursor = cursor;
            parsingContext.ParsingResult = parsingResult;
            parsingContext.ParsingSettings = parent.ParsingSettings;
            parsingContext.AttributeParser = parent.AttributeParser;
            parsingContext.AccessSpecifier = cursor.Kind == CXCursorKind.CXCursor_StructDecl ? EAccessSpecifier.Public : EAccessSpecifier.Private;
            parsingContext.StructClassTree = parent.StructClassTree;

            contextsStack.Push(parsingContext);
            return parsingContext;
        }

        protected override bool ShouldParseCurrentEntity(CXCursor cursor)
        {
            if (GetContext.RootCursor.Kind == CXCursorKind.CXCursor_StructDecl)
                return GetContext.ParsingSettings.ShouldParseAllStructs;
            else
                return GetContext.ParsingSettings.ShouldParseAllClasses;
        }

        protected override void SetParsedEntity(CXCursor cursor, List<AttributeInfo> attributeInfos)
        {
            ParsingResult.ParsedClass = new StructClassInfo(GetContext.RootCursor, attributeInfos, GetContext.RootCursor.Definition.IsNull);
        }

        protected override bool ShouldSkipPreAttributes(CXCursor cursor)
        {
            // Skip template parameters
            if (cursor.kind == CXCursorKind.CXCursor_TemplateTypeParameter ||
                cursor.kind == CXCursorKind.CXCursor_NonTypeTemplateParameter ||
                cursor.kind == CXCursorKind.CXCursor_TemplateTemplateParameter)
            {
                return true;
            }

            // Skip import/export symbol
            if (cursor.kind == CXCursorKind.CXCursor_DLLImport ||
                cursor.kind == CXCursorKind.CXCursor_DLLExport)
            {
                return true;
            }
            return false;
        }

        protected override CXChildVisitResult ParseNestedEntity(CXCursor cursor)
        {
            var visitResult = CXChildVisitResult.CXChildVisit_Continue;
            switch (cursor.Kind)
            {
            case CXCursorKind.CXCursor_CXXFinalAttr:
                SetClassIsFinal();
                break;

            case CXCursorKind.CXCursor_CXXAccessSpecifier:
                UpdateAccessSpecifier(cursor);
                break;

            case CXCursorKind.CXCursor_CXXBaseSpecifier:
                UpdateStructClassTree(cursor);
                AddBaseClass(cursor);
                break;

            case CXCursorKind.CXCursor_StructDecl:
            case CXCursorKind.CXCursor_ClassDecl:
                {
                    ClassParsingResult classResult;
                    visitResult = Parse(cursor, GetContext, out classResult);
                    if (classResult.ParsedClass != null)
                    {
                        if (classResult.ParsedClass.EntityType == EntityType.Struct)
                            ParsingResult.ParsedClass.NestedStructs.Add(new NestedStructClassInfo(classResult.ParsedClass));
                        else
                            ParsingResult.ParsedClass.NestedClasses.Add(new NestedStructClassInfo(classResult.ParsedClass));
                    }
                }
                break;

            case CXCursorKind.CXCursor_EnumDecl:
                {
                    EnumParsingResult result;
                    ClassParsingResult classResult = ParsingResult;
                    visitResult = _enumParser.Parse(cursor, GetContext, out result);
                    if (classResult.ParsedClass != null && result.ParsedEnum != null)
                    {
                        classResult.ParsedClass.NestedEnums.Add(result.ParsedEnum);
                    }
                    classResult.AppendResult(result);
                }
                break;

            case CXCursorKind.CXCursor_VarDecl:
            case CXCursorKind.CXCursor_FieldDecl:
                {
                    FieldParsingResult result;
                    ClassParsingResult classResult = ParsingResult;
                    visitResult = _fieldParser.Parse(cursor, GetContext, out result);
                    if (classResult.ParsedClass != null && result.ParsedField != null)
                    {
                        result.ParsedField.AccessSpecifier = GetContext.AccessSpecifier;
                        classResult.ParsedClass.Fields.Add(result.ParsedField);
                    }
                    classResult.AppendResult(result);
                }
                break;

            case CXCursorKind.CXCursor_CXXMethod:
                {
                    MethodParsingResult result;
                    ClassParsingResult classResult = ParsingResult;
                    visitResult = _methodParser.Parse(cursor, GetContext, out result);
                    if (classResult.ParsedClass != null && result.ParsedMethod != null)
                    {
                        result.ParsedMethod.AccessSpecifier = GetContext.AccessSpecifier;
                        classResult.ParsedClass.Methods.Add(result.ParsedMethod);
                    }
                    classResult.AppendResult(result);
                }
                break;
            }

            return visitResult;
        }

        protected override List<AttributeInfo>? GetCurrentEntityAttribute(CXCursor cursor)
        {
            if (GetContext.RootCursor.Kind == CXCursorKind.CXCursor_StructDecl)
                return GetContext.AttributeParser.GetStructAttributeInfos(cursor.Spelling.CString);
            else
                return GetContext.AttributeParser.GetClassAttributeInfos(cursor.Spelling.CString);
        }

        private void UpdateAccessSpecifier(CXCursor cursor)
        {
            GetContext.AccessSpecifier = GetEAccessSpecifier(cursor);
        }

        private static EAccessSpecifier GetEAccessSpecifier(CXCursor cursor)
        {
            switch (cursor.CXXAccessSpecifier)
            {
                case CX_CXXAccessSpecifier.CX_CXXPublic:
                    return EAccessSpecifier.Public;
                case CX_CXXAccessSpecifier.CX_CXXPrivate:
                    return EAccessSpecifier.Private;
                case CX_CXXAccessSpecifier.CX_CXXProtected:
                    return EAccessSpecifier.Protected;
            }
            return EAccessSpecifier.Invalid;
        }

        private void SetClassIsFinal()
        {
            var parsedClass = ParsingResult.ParsedClass;
            if (parsedClass != null)
            {
                parsedClass.ClassQualifiers.IsFinal = true;
            }
        }

        private void UpdateStructClassTree(CXCursor cursor)
        {
            var context = GetContext;
            if (context.StructClassTree == null)
                throw new Exception("Missing struct class tree.");

            UpdateStructClassTreeRecursion(context.RootCursor, cursor, context.StructClassTree);
        }

        private static unsafe void UpdateStructClassTreeRecursion(CXCursor childCursor, CXCursor baseOfCursor, StructClassTree structClassTree)
        {
            CXCursor baseCursorTypeDecl = baseOfCursor.Type.Declaration;
            if (structClassTree.AddInheritanceLink(
                EntityInfo.GetFullName(childCursor),
                EntityInfo.GetFullName(baseCursorTypeDecl),
                GetEAccessSpecifier(baseOfCursor)))
            {
                GCHandle treeHandle = GCHandle.Alloc(structClassTree);
                baseCursorTypeDecl.VisitChildren(BaseClassCursorVisitor, new CXClientData((IntPtr)treeHandle));
            }
        }

        private static unsafe CXChildVisitResult BaseClassCursorVisitor(CXCursor cursor, CXCursor parent, void* data)
        {
            CXClientData tmpData = (CXClientData)data;
            var tmpHandle = (GCHandle)tmpData.Handle;
            var classTree = tmpHandle.Target as StructClassTree;
            if (classTree == null)
                return CXChildVisitResult.CXChildVisit_Break;

            if (cursor.kind == CXCursorKind.CXCursor_CXXFinalAttr ||
                     cursor.kind == CXCursorKind.CXCursor_AnnotateAttr ||
                     cursor.kind == CXCursorKind.CXCursor_DLLImport ||
                     cursor.kind == CXCursorKind.CXCursor_DLLExport)
            {
                return CXChildVisitResult.CXChildVisit_Continue;
            }

            if (cursor.Kind == CXCursorKind.CXCursor_CXXBaseSpecifier)
            {
                UpdateStructClassTreeRecursion(parent, cursor, classTree);
                return CXChildVisitResult.CXChildVisit_Continue;
            }
            else
            {
                return CXChildVisitResult.CXChildVisit_Break;
            }
        }

        private void AddBaseClass(CXCursor cursor)
        {
            var parsedClass = ParsingResult.ParsedClass;
            if (parsedClass != null)
            {
                var parentInfo = new ClassParentInfo();
                parentInfo.InheritanceAccess = GetEAccessSpecifier(cursor);
                parentInfo.Type = new TypeInfo(cursor);
                parsedClass.Parent.Add(parentInfo);
            }
        }
    }
}
