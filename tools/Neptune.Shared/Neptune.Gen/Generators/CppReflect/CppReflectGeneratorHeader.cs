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
            /**
            *	Code will be generated at the very top of the generated header file (without macro).
            *	The code is then injected as soon as the generated header file is included in any other file.
            */
            HeaderFileHeader = 0,

            /**
            *	Code will be inserted in a macro generated for each tagged struct/class.
            *	The generated macro name is customizable using the MacroCodeGenUnitSettings::setClassFooterMacroPattern method.
            */
            ClassFooter,

            /**
            *	Code will be inserted in a macro to add at the very bottom of parsed header files.
            *	The generated macro name is customizable using the MacroCodeGenUnitSettings::setHeaderFileFooterMacroPattern method.
            */
            HeaderFileFooter,

            Count
        }
        private ECodeGenLocation CurrentLocation;
        private StringBuilder?[] LocationGenerated = new StringBuilder[(int)ECodeGenLocation.Count]; 

        public CppReflectGeneratorHeader(HeaderFile headerFile, CodeGenCppReflectSettings settings) :
            base(headerFile, settings)
        {
        }

        public string GetGeneratedFilePath(CodeGenFactory CodeGenFactory)
        {
            var path = Path.Combine("Reflect", Settings.GetGeneratedHeaderFileName(HeaderFile.FilePath));
            return CodeGenFactory.GetGeneratedFilePath(HeaderFile, path);
        }

        protected override ETraversalBehaviour GenerateCodeForEntity(EntityInfo entityInfo)
        {
            using BorrowStringBuilder stringBuilderPool = new(StringBuilderCache.Big);
            for (int i = 0; i < (int)ECodeGenLocation.Count; i++)
            {
                CurrentLocation = (ECodeGenLocation)i;

                if (CurrentLocation == ECodeGenLocation.ClassFooter)
                {
                    if (entityInfo.EntityType == EntityType.Class ||
                        entityInfo.EntityType == EntityType.Struct)
                    {
                    }
                }
                else
                {
                    ETraversalBehaviour ret = ETraversalBehaviour.AbortWithFailure;
                    switch (CurrentLocation)
                    {
                    case ECodeGenLocation.HeaderFileHeader:

                        break;

                    case ECodeGenLocation.HeaderFileFooter:
           
                        break;
                    }
                    return ret;
                }
            }

            return ETraversalBehaviour.Recurse;
        }

        private ETraversalBehaviour GenerateHeaderFileFooterCodeForEntity(EntityInfo entityInfo)
        {
	        return ETraversalBehaviour.AbortWithSuccess;
        }

    public override void Generate(CodeGenFactory CodeGenFactory)
        {
            var parsingResult = HeaderFile.FileParsingResult;
            if (parsingResult.Errors.Count > 0)
                return;

            for (int i = 0; i < (int)ECodeGenLocation.Count; i++)
            {
                LocationGenerated[i] = StringBuilderCache.Big.Borrow();
            }

            var ret = ForeachEntityPair();
            if (ret != ETraversalBehaviour.AbortWithFailure)
            {
                using BorrowStringBuilder borrower = new(StringBuilderCache.Big);
                StringBuilder builder = borrower.StringBuilder;

                builder.Append("#pragma once\n");
                builder.Append(LocationGenerated[(int)ECodeGenLocation.HeaderFileHeader]);


                var filePath = GetGeneratedFilePath(CodeGenFactory);
                {
                    GenBufferHandle bufferHandle = new GenBufferHandle(builder);
                    StringView contentView = new(bufferHandle.Buffer.Memory);
                    CodeGenFactory.WriteFileIfChanged(filePath, contentView);
                }
            }

            for (int i = 0; i < (int)ECodeGenLocation.Count; i++)
            {
                if (LocationGenerated[i] != null)
                {
                    StringBuilderCache.Big.Return(LocationGenerated[i]);
                    LocationGenerated[i] = null;
                }
            }
        }
    }
}
