using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace Neptune.Gen
{
    internal class CppReflectGeneratorSource : CppReflectGenerator
    {
        private List<EnumInfo> _registeredEnums = new List<EnumInfo>();
        private List<StructClassInfo> _registeredClasses = new List<StructClassInfo>();

        public CppReflectGeneratorSource(HeaderFile headerFile, CodeGenCppReflectSettings settings) :
            base(headerFile, settings)
        {
        }

        public string GetGeneratedFilePath(CodeGenFactory CodeGenFactory)
        {
            var path = Path.Combine("Reflect", Settings.GetGeneratedSourceFileName(HeaderFile.FilePath));
            return CodeGenFactory.GetGeneratedFilePath(HeaderFile, path);
        }

        protected override ETraversalBehaviour GenerateCodeForEntity(StringBuilder builder, EntityInfo entityInfo)
        {
            var ret = ETraversalBehaviour.Recurse;
            switch (entityInfo.EntityType)
            {
                case EntityType.Enum:
                    AppendEnum(builder, entityInfo as EnumInfo);
                    ret = ETraversalBehaviour.Continue; // Go to next enum
                    break;
                case EntityType.Field:
                case EntityType.Method:
                    ret = ETraversalBehaviour.Continue; // Go to next enum
                    break;

                case EntityType.Struct:
                case EntityType.Class:
                    var classInfo = entityInfo as StructClassInfo;
                    if (classInfo != null && classInfo.TypeInfo != null && !classInfo.IsForwardDecl)
                    {
                        AppendClass(builder, classInfo);
                        ret = ETraversalBehaviour.Recurse;
                        break;
                    }

                    ret = ETraversalBehaviour.Continue; // Go to next enum
                    break;

                default:
                    Log.Error($"The entity {entityInfo.FullName} has an undefined type.Abort.");
                    return ETraversalBehaviour.AbortWithFailure;
            }
            return ret;
        }

        private void AppendEnum(StringBuilder builder, EnumInfo? enumInfo)
        {
            if (enumInfo == null)
                return;

            string enumTypeName = enumInfo.Type.CanonicalName;
            builder.Append("NEnum const* GetStaticEnum_").Append(enumTypeName).Append("() \r\n");
            builder.Append("{  \r\n");
            {
                builder.Append("static bool initialized = false; \r\n");
                builder.Append("static NEnum type(\"").Append(enumInfo.Name).Append("\", ").Append(ComputeEntityID(enumInfo)).Append(", GetArchetype<").Append(enumInfo.UnderlyingType.CanonicalName).Append(">()); \r\n");
                builder.Append("if (!initialized) { \r\n");
                {
                    builder.Append("initialized = true; \r\n");

                    if (enumInfo.EnumValues.Count > 0)
                    {
                        builder.Append("EnumValue* enumValue = nullptr; \r\n");
                        builder.Append("type.SetEnumValuesCapacity(").Append(enumInfo.EnumValues.Count).Append("); \r\n");
                        foreach (var enumValue in enumInfo.EnumValues)
                        {
                            builder.Append("enumValue = type.AddEnumValue(").Append(enumValue.Name).Append(", ").Append(ComputeEntityID(enumValue)).Append(enumValue.EnumValue).Append("); \r\n");
                        }
                    }
                }
                builder.Append("} \r\n");
                builder.Append("return &type; \r\n");
            }
            builder.Append("} \r\n");

            builder.Append("template<> NEnum const* StaticEnum<").Append(enumTypeName).Append(">() \r\n");
            builder.Append("\t{\r\n");
            builder.Append("\t\treturn GetStaticEnum_").Append(enumTypeName).Append(";\r\n");
            builder.Append("\t}\r\n");

            _registeredEnums.Add(enumInfo);
        }

        private void AppendClass(StringBuilder builder, StructClassInfo? classInfo)
        {
            if (classInfo == null)
                return;

            if (classInfo.TypeInfo.IsTemplateType)
            {
                // TODO Support template type
                throw new NotImplementedException();
            }
            else
            {
                AppendClassConstructMethod(builder, classInfo);
                AppendClassGeneratedInitCode(builder, classInfo);
            }

            // Define the registrator only when there is no outer entity. 
            if (classInfo.Outer == null)
                _registeredClasses.Add(classInfo);
        }

        private void AppendClassConstructMethod(StringBuilder builder, StructClassInfo classInfo)
        {
            builder.Append("NClass* ConstructClassTypeInfo_").Append(classInfo.FullName).Append("()\r\n");
            builder.Append("{\r\n");
            builder.Append("\tstatic NClass* thisClass = ").Append(classInfo.FullName).Append("::StaticGetArchetype();\r\n");
            builder.Append("\tif (thisClass && thisClass->constructed == false)\r\n");
            builder.Append("\t{\r\n");
            builder.Append("\t\tthisClass->constructed = true;\r\n");

            FillEntityProperties(builder, classInfo);
            FillClassParents(builder, classInfo);
            FillClassFields(builder, classInfo);
            FillClassMethods(builder, classInfo);

            builder.Append("\t}\r\n");
            builder.Append("\treturn thisClass;\r\n");
            builder.Append("}\r\n");
        }

        protected void FillClassParents(StringBuilder builder, EntityInfo entityInfo)
        {
        }

        protected void FillClassFields(StringBuilder builder, EntityInfo entityInfo)
        {
        }

        protected void FillClassMethods(StringBuilder builder, EntityInfo entityInfo)
        {
        }

        protected void FillClassNestedArchetypes(StringBuilder builder, EntityInfo entityInfo)
        {
            // TODO
            throw new NotImplementedException();
        }

        private void AppendClassGeneratedInitCode(StringBuilder builder, StructClassInfo classInfo)
        {
            string cannonicalName = classInfo.TypeInfo.CanonicalName;

            builder.Append("static NClass* ").Append(cannonicalName).Append("::StaticGetPrivateArchetype()\r\n");
            {
                builder.Append("{\r\n");
                builder.Append("\t static NClass* classType = nullptr;\r\n");
                builder.Append("\t if(classType == nullptr) {\r\n");

                builder.Append("\t\t StaticGetPrivateArchetypeImpl(\r\n");
                builder.Append("\t\t\t classType,\r\n");
                builder.Append("\t\t\t \"").Append(classInfo.Name).Append("\",\r\n");
                builder.Append("\t\t\t").Append(ComputeEntityID(classInfo)).Append(",\r\n");
                builder.Append("\t\t\t sizeof(").Append(classInfo.Name).Append("),\r\n");
                builder.Append("\t\t\t alignof(").Append(classInfo.Name).Append(")\r\n");

                builder.Append("\t );}\r\n");
                builder.Append("\t return classType; \r\n");
            }
            builder.Append("}\r\n");

            string classType = classInfo.TypeInfo.GetName();
            builder.Append("template<> ").Append(ModuleAPI).Append("NClass* StaticArchetype<").Append(classType).Append(">()\r\n");
            builder.Append("{\r\n");
            builder.Append("\treturn ").Append(classType).Append("::StaticClass();\r\n");
            builder.Append("}\r\n");
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
            builder.Append("#include \"NObject/GeneratedCppIncludes.h\"\r\n");
            builder.Append("#include \"").Append(HeaderFile.FilePath).Append("\"\r\n");

            // Generate HeaderFile main content
            var ret = ForeachEntityPair(builder);
            if (ret == ETraversalBehaviour.AbortWithFailure)
                return;

            // Generate automatic register
            bool hasRegisteredEnums = _registeredEnums.Count > 0;
            bool hasRegisteredClasses = _registeredClasses.Count > 0;
            if (hasRegisteredEnums)
            {
                builder.Append($"namespace {HeaderFileID}_Generated {{  \r\n");
                {
                    string staticsName = $"Static_Register_Info";
                    int combinedHash = Int32.MaxValue;

                    builder.Append("\tstruct ").Append(staticsName).Append("\r\n");
                    builder.Append("\t{\r\n");
                    {
                        if (hasRegisteredEnums)
                            builder.Append("\t\tstatic const EnumTypeRegisterInfo EnumInfos[];\r\n");

                        if (hasRegisteredClasses)
                            builder.Append("\t\tstatic const ClassTypeRegisterInfo ClassInfos[];\r\n");
                    }
                    builder.Append("\t};\r\n");

                    if (hasRegisteredEnums)
                    {
                        builder.Append("\tconst EnumTypeRegisterInfo ").Append(staticsName).Append("::EnumInfos[] = {\r\n");
                        foreach (var enumInfo in _registeredEnums)
                        {
                            int hash = ComputeEntityID(enumInfo);
                            builder.Append("\t\t{ ");
                            builder.Append("GetStaticEnum_").Append(enumInfo.Type.CanonicalName);
                            builder.Append("},\r\n");

                            combinedHash = HashCode.Combine(combinedHash, hash);
                        }
                        builder.Append("\t};\r\n");
                    }

                    if (hasRegisteredClasses)
                    {
                        builder.Append("\tconst ClassTypeRegisterInfo ").Append(staticsName).Append("::ClassInfos[] = {\r\n");
                        foreach (var classInfo in _registeredClasses)
                        {
                            int hash = ComputeEntityID(classInfo);
                            builder.Append("\t\t{ ");
                            builder.Append("ConstructClassTypeInfo_").Append(classInfo.FullName).Append(",");
                            builder.Append(classInfo.FullName).Append("::StaticGetArchetype");
                            builder.Append("},\r\n");

                            combinedHash = HashCode.Combine(combinedHash, hash);
                        }
                        builder.Append("\t};\r\n");
                    }

                    builder.Append("\t static ArchetypeRegisterer const registerer_").Append(combinedHash).Append("(");
                    builder.Append("\t\t").AppendArray(!hasRegisteredEnums, staticsName, "EnumInfos").Append(",\r\n");
                    builder.Append("\t\t").AppendArray(!hasRegisteredClasses, staticsName, "ClassInfos").Append("\r\n");
                    builder.Append("); \r\n");
                }
                builder.Append("} \r\n");
            }

            // Write generated file
            var filePath = GetGeneratedFilePath(CodeGenFactory);
            {
                GenBufferHandle bufferHandle = new GenBufferHandle(builder);
                StringView contentView = new(bufferHandle.Buffer.Memory);
                CodeGenFactory.WriteFileIfChanged(filePath, contentView);
            }
        }
    }
}
