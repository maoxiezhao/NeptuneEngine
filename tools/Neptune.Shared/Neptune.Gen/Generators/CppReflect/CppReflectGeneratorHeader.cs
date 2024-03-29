﻿using ClangSharp;
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
            if (structClassInfo == null || structClassInfo.TypeInfo == null)
                return ETraversalBehaviour.AbortWithFailure;

            //Do not generate code for forward declarations
            if (structClassInfo.IsForwardDecl)
                return ETraversalBehaviour.Continue;

            List<string> generatedMacors = new List<string>();
            if (structClassInfo.TypeInfo.IsTemplateType)
            {
                // TODO Support template type
                throw new NotImplementedException();
            }
            else
            {
                AppendRegisterChildClassMethod(builder, structClassInfo, generatedMacors);
                AppendClassConstructors(builder, structClassInfo, generatedMacors);
                AppendClassGeneratedBody(builder, structClassInfo, generatedMacors);
            }

            // Append generated body macro block
            using (CodeGenMacroCreator macro = new CodeGenMacroCreator(builder, this, structClassInfo, GeneratedBodyMacroSuffix))
            {
                builder.Append("public: \\\r\n");
                foreach (var macroName in generatedMacors)
                    builder.Append('\t').AppendMacroName(this, structClassInfo, macroName).Append(" \\\r\n");
                builder.Append("private: \\\r\n");
                builder.Append(" \\\r\n");
            }

            return ETraversalBehaviour.Recurse;
        }

        private void AppendClassConstructors(StringBuilder builder, StructClassInfo classInfo, List<string> generatedMacors)
        {
            using (CodeGenMacroCreator macro = new CodeGenMacroCreator(builder, this, classInfo, "CONSTRUCTORS"))
            {
                // Default constructor

                // Disable move and copy constructors
                builder.Append("private: \\\r\n");
                builder.Append("\t/** Private move- and copy-constructors, should never be used */ \\\r\n");
                builder.Append('\t').Append(classInfo.Name).Append('(').Append(classInfo.Name).Append("&&); \\\r\n");
                builder.Append('\t').Append(classInfo.Name).Append("(const ").Append(classInfo.Name).Append("&); \\\r\n");
                builder.Append("public: \\\r\n");

                generatedMacors.Add(macro.MacroSuffix);
            }
        }

        private void AppendRegisterChildClassMethod(StringBuilder builder, StructClassInfo classInfo, List<string> generatedMacors)
        {
            // Register child class infos. TODO Do it in generated source file

            using (CodeGenMacroCreator macro = new CodeGenMacroCreator(builder, this, classInfo, "REGISTER_CHILD"))
            {
                builder.Append("private: \\\r\n");

                // Register the child to the subclasses list
                
                builder.Append("template <typename ChildClass> static void RegisterChildClass(NClass* childClass) noexcept { \\\r\n");
                builder.Append("NClass* thisClass = StaticGetArchetype(); \\\r\n");
                builder.Append("if constexpr (!std::is_same_v<ChildClass, ").Append(classInfo.Name).Append(">) \\\r\n");
                builder.Append("\tthisClass->AddSubclass(childClass, CodeGenerationHelpers::ComputeClassPointerOffset<ChildClass, ").Append(classInfo.Name).Append(">()); \\\r\n");
                builder.Append("else { \\\r\n");

                // Reserve the correct amount of memory for fields and static fields
                int fieldsCount = 0;
                int staticFieldsCount = 0;
                if (classInfo.Fields.Count > 0)
                {
                    // First loop to get the amount of fields
                    foreach (var field in classInfo.Fields)
                    {
                        if (field.IsStatic)
                            staticFieldsCount++;
                        else
                            fieldsCount++;
                    }
                    builder.Append("childClass->SetFieldsCapacity(").Append(fieldsCount.ToString()).Append("u); \\\r\n");
                    builder.Append("childClass->SetStaticFieldsCapacity(").Append(staticFieldsCount.ToString()).Append("u); \\\r\n");
                    builder.Append("} \\\r\n");

                    // Add field infos
                    builder.Append("[[maybe_unused]] NField* field = nullptr; [[maybe_unused]] NStaticField* staticField = nullptr; \\\r\n");
                    foreach (var field in classInfo.Fields)
                        AppendFieldProperties(builder, classInfo, field);
                }
                else
                {
                    builder.Append("} \\\r\n");
                }

                // Propagate the child class registration to parent classes too
                if (classInfo.Parent.Count > 0)
                {
                    foreach (var parentInfo in classInfo.Parent)
                    {
                        builder.Append("CodeGenerationHelpers::registerChildClass<").Append(parentInfo.Type.GetName()).Append(", ChildClass>(childClass); \\\r\n");
                    }
                }

                builder.Append("public: \\\r\n");
            }
        }

        private void AppendFieldProperties(StringBuilder builder, StructClassInfo classInfo, FieldInfo fieldInfo)
        {
            if (fieldInfo.IsStatic)
            {
                builder.Append("field = childClass.AddStaticField(\"").Append(fieldInfo.Name).Append("\",")     // Field name
                       .Append(ComputeClassNestedEntityID("ChildClass", fieldInfo)).Append(", ")                // Field ID
                       .Append("GetType<").Append(fieldInfo.Type.GetName()).Append(">(), ")                     // Field type info
                       .Append("static_cast<EFieldFlags>(").Append(ComputeFieldFlags(fieldInfo)).Append("), ")  // Field flags
                       .Append("&").Append(classInfo.Name).Append("::").Append(fieldInfo.Name).Append(", ")     // Memory offset
                       .Append("thisClass);\\\r\n");                                                            // Outer
            }
            else
            {
                builder.Append("field = childClass.AddField(\"").Append(fieldInfo.Name).Append("\",")           // Field name
                       .Append(ComputeClassNestedEntityID("ChildClass", fieldInfo)).Append(", ")                // Field ID
                       .Append("GetType<").Append(fieldInfo.Type.GetName()).Append(">(), ")                     // Field type info
                       .Append("static_cast<EFieldFlags>(").Append(ComputeFieldFlags(fieldInfo)).Append("), ")  // Field flags
                       .Append("offsetof(childClass,").Append(fieldInfo.Name).Append("), ")                     // Memory offset
                       .Append("thisClass);\\\r\n");                                                            // Outer
            }
        }

        private void AppendClassGeneratedBody(StringBuilder builder, StructClassInfo classInfo, List<string> generatedMacors)
        {
            using (CodeGenMacroCreator macro = new CodeGenMacroCreator(builder, this, classInfo, "INCLASS"))
            {
                builder.Append("private: \\\r\n");
                builder.Append(ModuleAPI).Append(" static NClass* StaticGetPrivateArchetype();\\\r\n");

                builder.Append("public: \\\r\n");
                builder.Append("inline static NClass* StaticGetArchetype() \\\r\n");
                builder.Append("{ \\\r\n");
                builder.Append("return StaticGetPrivateArchetype(); \\\r\n");
                builder.Append("} \\\r\n");

                builder.Append("typedef ").Append(classInfo.Name).Append(" ThisClass; \\\r\n");

                if (classInfo.Parent.Count > 0)
                {
                    // Typedef Super for the first parent class
                    // TODO: Prohibit multiple inheritance
                    builder.Append("typedef ").Append(classInfo.Parent[0].Type.GetName()).Append(" Super; \\\r\n");
                }
            }
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
            builder.Append("template<> ").Append(ModuleAPI).Append("NClass* StaticArchetype<class ").Append(structClassInfo.TypeInfo.GetName()).Append(">();\r\n");
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
