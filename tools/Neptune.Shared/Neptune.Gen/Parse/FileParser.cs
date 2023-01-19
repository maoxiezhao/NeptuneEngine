using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class FileParser
    {
        private ParsingSettings _settings;

        public FileParser(ParsingSettings settings)
        {
            _settings = settings;
        }

        public void Parse(string filePath, FileParsingResult result)
        {
            if (!File.Exists(filePath))
            {
                result.Errors.Add(new ParsingError( string.Format("File {0} doesn't exist.", filePath)));
                return;
            }

            var commandLindArgs = _settings.GetCompilationArguments();
            var translationFlags = CXTranslationUnit_Flags.CXTranslationUnit_None;

            var index = CXIndex.Create();
            var translationUnitError = CXTranslationUnit.TryParse(index, filePath, commandLindArgs.ToArray(), Array.Empty<CXUnsavedFile>(), translationFlags, out var handle);
            
        }

        public object Clone()
        {
            var clone = new FileParser(_settings);
            return clone;
        }
    }
}
