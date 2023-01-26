using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class EnumValueParsingResult : ParsingResultBase
    {
        public EnumValueInfo? ParsedEnumValue = null;
    }

    public class EnumParsingResult : ParsingResultBase
    {
        public EnumInfo? ParsedEnum = null;
    }
}
