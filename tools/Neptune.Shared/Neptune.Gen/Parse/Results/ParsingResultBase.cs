using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class ParsingResultBase
    {
        public List<ParsingError> Errors = new List<ParsingError>();

        public void AppendResult(ParsingResultBase otherResult)
        {
            Errors.AddRange(otherResult.Errors);
        }
    }
}
