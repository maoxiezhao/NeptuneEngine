using ClangSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class CodeGenSettings
    {
        public static string FilenameTag         = "##FILENAME##";

        public string GeneratedHeaderFileNamePattern = "##FILENAME##.gen.h";
        public string GeneratedSourceFileNamePattern = "##FILENAME##.gen.cpp";

        public string GetGeneratedHeaderFileName(string sourceFile)
        {
            string filename = GeneratedHeaderFileNamePattern;
            filename.Replace(FilenameTag, Path.GetFileName(sourceFile));
            return filename;
        }

        public string GetGeneratedSourceFileName(string sourceFile)
        {
            string filename = GeneratedSourceFileNamePattern;
            filename.Replace(FilenameTag, Path.GetFileName(sourceFile));
            return filename;
        }
    }
}
