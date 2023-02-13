using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public struct CSharpProject
    {
        /// <summary>
        /// The system libraries references.
        /// </summary>
        public HashSet<string> SystemReferences;

        /// <summary>
        /// The .Net libraries references (dll or exe files paths).
        /// </summary>
        public HashSet<string> FileReferences;

        /// <summary>
        /// The output folder path (optional).
        /// </summary>
        public string OutputPath;

        /// <summary>
        /// The intermediate output folder path (optional).
        /// </summary>
        public string IntermediateOutputPath;
    }
}
