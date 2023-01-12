using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class CodeGenUnit
    {
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
    }
}
