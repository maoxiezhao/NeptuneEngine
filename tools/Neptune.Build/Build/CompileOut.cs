using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class CompileOutput
    {
        /// <summary>
        /// The result object files.
        /// </summary>
        public readonly List<string> ObjectFiles = new List<string>();

        /// <summary>
        /// The result debug data files.
        /// </summary>
        public readonly List<string> DebugDataFiles = new List<string>();
    }
}
