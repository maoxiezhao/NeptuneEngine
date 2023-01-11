using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class FileParser
    {
        private ParsingSettings _parsingSettings = null;

        public void SetParsingSettings(ParsingSettings parsingSettings)
        {
            _parsingSettings = parsingSettings;
        }
    }
}
