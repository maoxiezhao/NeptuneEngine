using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class CodeGenUnit
    {
        public CodeGenSettings Settings = new CodeGenSettings();

        public bool GenerateCode(FileParsingResult fileParsingResult)
        {
            return false;
        }

        public object Clone()
        {
            var clone = new CodeGenUnit
            {
            };
            return clone;
        }

        public string GetGeneratedHeaderFilePath(string sourceFile)
        {
            return Path.Combine(Globals.Output, Settings.GetGeneratedHeaderFileName(sourceFile));
        }

        public string GetGeneratedSourceFilePath(string sourceFile)
        {
            return Path.Combine(Globals.Output, Settings.GetGeneratedSourceFileName(sourceFile));
        }
    }
}
