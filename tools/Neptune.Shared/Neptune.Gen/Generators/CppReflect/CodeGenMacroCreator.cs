using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Emit;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    internal static class HeaderCodeGeneratorStringBuilderExtensions
    {
        public static StringBuilder AppendMacroName(this StringBuilder builder, string fileId, int lineNumber, string macroSuffix)
        {
            return builder.Append(fileId).Append('_').Append(lineNumber).Append('_').Append(macroSuffix);
        }

        public static StringBuilder AppendMacroName(this StringBuilder builder, CppReflectGenerator generator, StructClassInfo classObj, string macroSuffix)
        {
            return builder.AppendMacroName(generator.HeaderFileID, classObj.DeclarationLineNumber, macroSuffix);
        }
    }

    public class CodeGenMacroCreator : IDisposable
    {
        private readonly StringBuilder _builder;
        private readonly int _startingLength;

        public CodeGenMacroCreator(StringBuilder builder, CppReflectGenerator generator, StructClassInfo structClassInfo, string macroSuffix)
        {
            builder.Append("#define ").AppendMacroName(generator, structClassInfo, macroSuffix).Append(" \\\r\n");
            _builder = builder;
            _startingLength = builder.Length;
        }

        public void Dispose()
        {
            int finalLength = _builder.Length;
            _builder.Length -= 4;
            if (finalLength == _startingLength)
                _builder.Append("\r\n");
            else
                _builder.Append("\r\n\r\n");
        }
    }
}
