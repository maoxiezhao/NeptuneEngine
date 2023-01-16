using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class FileParser
    {
        public ParsingSettings Settings = new ParsingSettings();

        public void Parse(string filePath, FileParsingResult result)
        { 
        }

        public object Clone()
        {
            var clone = new FileParser
            {
                Settings = Settings,
            };
            return clone;
        }
    }
}
