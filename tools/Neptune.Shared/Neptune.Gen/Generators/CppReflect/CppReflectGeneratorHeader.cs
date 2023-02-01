using ClangSharp;
using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class CppReflectGeneratorHeader : CppReflectGenerator
    {
        private enum ECodeGenLocation
        {
            HeaderFileHead,
            HeaderFileBody,
            HeaderFileFoot,

            Count
        }
        private ECodeGenLocation CurrentLocation;

        public CppReflectGeneratorHeader(HeaderFile headerFile, CodeGenCppReflectSettings settings) :
            base(headerFile, settings)
        {
        }

        public string GetGeneratedFilePath(CodeGenFactory CodeGenFactory)
        {
            var path = Path.Combine("Reflect", Settings.GetGeneratedHeaderFileName(HeaderFile.FilePath));
            return CodeGenFactory.GetGeneratedFilePath(HeaderFile, path);
        }

        private ETraversalBehaviour GenerateCodeForStructClass(StringBuilder builder, StructClassInfo? structClassInfo)
        {
            if (structClassInfo == null)
                return ETraversalBehaviour.AbortWithFailure;

            if (structClassInfo.IsForwardDecl)
                return ETraversalBehaviour.Continue;

            using (CodeGenMacroCreator macro = new CodeGenMacroCreator(builder, this, structClassInfo, GeneratedBodyMacroSuffix))
            {
                builder.Append("public: \\\r\n");


                builder.Append(" \\\r\n");
            }

            return ETraversalBehaviour.Recurse;
        }

        protected ETraversalBehaviour GenerateHeaderFileBodyCodeForEntity(StringBuilder builder, EntityInfo entityInfo)
        {
            var ret = ETraversalBehaviour.Recurse;
            switch (entityInfo.EntityType)
            {
                case EntityType.Enum:
                    ret = ETraversalBehaviour.Continue; // Go to next enum
                    break;
                case EntityType.Field:
                case EntityType.Method:
                    ret = ETraversalBehaviour.Break; //Don't need to iterate over those individual entities
                    break;

                case EntityType.Struct:
                case EntityType.Class:
                    ret = GenerateCodeForStructClass(builder, entityInfo as StructClassInfo);
                    break;

                default:
                    Log.Error($"The entity {entityInfo.FullName} has an undefined type.Abort.");
                    return ETraversalBehaviour.AbortWithFailure;
            }
            return ret;
        }

        protected ETraversalBehaviour GenerateHeaderFileFooterCodeForEntity(StringBuilder builder, EntityInfo entityInfo)
        {
            var ret = ETraversalBehaviour.Recurse;
            switch (entityInfo.EntityType)
            {
                case EntityType.Enum:
                    DeclareEnumTemplateSpecialization(builder, entityInfo as EnumInfo);
                    ret = ETraversalBehaviour.Continue;
                    break;

                case EntityType.Struct:
                case EntityType.Class:
                    var classInfo = entityInfo as StructClassInfo;
                    if (classInfo != null && classInfo.TypeInfo != null && !classInfo.IsForwardDecl)
                    {
                        DeclareClassTemplateSpecialization(builder, classInfo);
                        break;
                    }

                    ret = ETraversalBehaviour.Continue;
                    break;

                case EntityType.Field:
                case EntityType.Method:
                    //Don't need to iterate over those individual entities
                    ret = ETraversalBehaviour.Break;
                    break;

                default:
                    Log.Error($"The entity { entityInfo.FullName } has an undefined type.Abort.");
                    return ETraversalBehaviour.AbortWithFailure;
            }
            return ret;
        }

        protected override ETraversalBehaviour GenerateCodeForEntity(StringBuilder builder, EntityInfo entityInfo)
        {
            if (CurrentLocation == ECodeGenLocation.HeaderFileBody)
                return GenerateHeaderFileBodyCodeForEntity(builder, entityInfo);
            else
                return GenerateHeaderFileFooterCodeForEntity(builder, entityInfo);
        }

        public override void Generate(CodeGenFactory CodeGenFactory)
        {
            if (HeaderFile.ModuleInfo == null)
                return;

            var parsingResult = HeaderFile.FileParsingResult;
            if (parsingResult.Errors.Count > 0)
                return;

            using BorrowStringBuilder borrower = new(StringBuilderCache.Big);
            StringBuilder builder = borrower.StringBuilder;

            // Include HeaderFile Headers
            builder.Append("#include \"NObject/ObjectMacros.h\"\r\n");

            string strippedName = Path.GetFileNameWithoutExtension(HeaderFile.FilePath);
            string defineName = $"{HeaderFile.ModuleInfo.ModuleName.ToUpper()}_{strippedName.ToUpper()}_GENERATED_H";
            builder.Append("#ifdef ").Append(defineName).Append("\r\n");
            builder.Append("#error \"").Append(strippedName).Append(".generated.h already included, missing '#pragma once' in ").Append(strippedName).Append(".h\"\r\n");
            builder.Append("#endif\r\n");
            builder.Append("#define ").Append(defineName).Append("\r\n");
            builder.Append("\r\n");

            // Generate HeaderFile main content
            for (int i = 0; i < (int)ECodeGenLocation.Count; i++)
            {
                CurrentLocation = (ECodeGenLocation)i;
                var ret = ForeachEntityPair(builder);
                if (ret == ETraversalBehaviour.AbortWithFailure)
                    return;

                builder.Append("\r\n");
            }

            // Write generated file
            var filePath = GetGeneratedFilePath(CodeGenFactory);
            {
                GenBufferHandle bufferHandle = new GenBufferHandle(builder);
                StringView contentView = new(bufferHandle.Buffer.Memory);
                CodeGenFactory.WriteFileIfChanged(filePath, contentView);
            }
        }

        private void DeclareClassTemplateSpecialization(StringBuilder builder, StructClassInfo? structClassInfo)
        {
            if (structClassInfo == null || structClassInfo.TypeInfo == null) return;
            builder.Append("template<> ").Append(ModuleAPI).Append("Archetype const* StaticArchetype<class ").Append(structClassInfo.TypeInfo.GetName()).Append(">();\r\n");
            builder.Append("\r\n");
        }

        private void DeclareEnumTemplateSpecialization(StringBuilder builder, EnumInfo? enumInfo)
        {
            if (enumInfo == null || enumInfo.Type == null) return;
            builder.Append("\r\n");
            builder.Append("enum class ").Append(enumInfo.Type.CanonicalName);
#if FALSE
            if (enumInfo.UnderlyingType != null)
            {
                builder.Append(" : ").Append(enumInfo.UnderlyingType.GetName());
            }
#endif
            builder.Append(";\r\n");

            // Forward declare the StaticEnum<> specialization for enum classes
            builder.Append("template<> ").Append(ModuleAPI).Append(" NEnum const* StaticEnum<").Append(enumInfo.Type.CanonicalName).Append(">() noexcept;\r\n");
            builder.Append("\r\n");
        }
    }
}
