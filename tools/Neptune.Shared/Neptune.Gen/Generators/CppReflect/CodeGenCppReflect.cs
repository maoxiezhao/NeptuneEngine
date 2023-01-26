using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace Neptune.Gen
{
    public class CodeGenCppReflectSettings : CodeGenSettings
    {
        public CodeGenCppReflectSettings()
        {
            GeneratedHeaderFileNamePattern = FilenameTag + ".gen.h";
            GeneratedSourceFileNamePattern = FilenameTag + ".gen.cpp";
        }
    }

    public partial class CodeGenCppReflect : CodeGenUnit
    {
        [CodeGen(Name = "CodeGen", Description = "Engine cpp reflect code generation")]
        public static CodeGenUnit CodeGenerator(CodeGenFactory factory)
        {
            return new CodeGenCppReflect(factory, new CodeGenCppReflectSettings());
        }

        public CodeGenCppReflect(CodeGenFactory factory, CodeGenSettings settings) :
            base(factory, settings)
        {
        }

        public override void GenerateCode(HeaderFile headerFile)
        {
            var parsingResult = headerFile.FileParsingResult;
            if (parsingResult.Errors.Count > 0)
                return;

            var settings = _settings as CodeGenCppReflectSettings;
            if (settings == null)
                return;

            (new CppReflectGeneratorHeader(headerFile, settings)).Generate(_codeGenFactory);
            (new CppReflectGeneratorSource(headerFile, settings)).Generate(_codeGenFactory);
        }
    }
}
