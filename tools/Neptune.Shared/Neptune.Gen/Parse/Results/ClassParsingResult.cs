using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class ClassParsingResult : ParsingResultBase
    {
        public StructClassInfo? ParsedClass { get; set; } = null;
    }
}
