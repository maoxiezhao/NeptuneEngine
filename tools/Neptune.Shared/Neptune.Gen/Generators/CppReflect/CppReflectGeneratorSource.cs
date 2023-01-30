using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    internal class CppReflectGeneratorSource : CppReflectGenerator
    {
        public CppReflectGeneratorSource(HeaderFile headerFile, CodeGenCppReflectSettings settings) :
            base(headerFile, settings)
        {
        }

        public string GetGeneratedFilePath(CodeGenFactory CodeGenFactory)
        {
            var path = Path.Combine("Reflect", Settings.GetGeneratedSourceFileName(HeaderFile.FilePath));
            return CodeGenFactory.GetGeneratedFilePath(HeaderFile, path);
        }

        protected override ETraversalBehaviour GenerateCodeForEntity(EntityInfo entityInfo)
        {
            return ETraversalBehaviour.Recurse;
        }

        public override void Generate(CodeGenFactory CodeGenFactory)
        {
            var parsingResult = HeaderFile.FileParsingResult;
            if (parsingResult.Errors.Count > 0)
                return;

            using BorrowStringBuilder borrower = new(StringBuilderCache.Big);
            StringBuilder builder = borrower.StringBuilder;


            var filePath = GetGeneratedFilePath(CodeGenFactory);
            {
                GenBufferHandle bufferHandle = new GenBufferHandle(builder);
                StringView contentView = new(bufferHandle.Buffer.Memory);
                CodeGenFactory.WriteFileIfChanged(filePath, contentView);
            }
        }
    }
}
