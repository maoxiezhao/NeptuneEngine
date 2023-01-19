using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class ParsingError
    {
        public string ErrorMsg { get; }

        public ParsingError(string msg)
        {
            ErrorMsg = msg;
        }
    }
}
