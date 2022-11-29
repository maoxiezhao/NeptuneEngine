using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public enum LinkerOutput
    {
        /// <summary>
        /// The executable file (aka .exe file).
        /// </summary>
        Executable,

        /// <summary>
        /// The shared library file (aka .dll file).
        /// </summary>
        SharedLibrary,

        /// <summary>
        /// The static library file (aka .lib file).
        /// </summary>
        StaticLibrary,
    }
}
