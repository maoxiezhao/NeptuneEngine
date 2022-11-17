using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class BuildOptions
    {
        public Target Target;
        public Platform Platform;
        public HashSet<string> Definitions = new HashSet<string>();
        public List<string> SourceFiles = new List<string>();
        public string IntermediateFolder;
        public string OutputFolder;
        public HashSet<string> Libraries = new HashSet<string>();

        /// <summary>
        /// The collection of the modules that are required by this module (for linking). Inherited by the modules that include it.
        /// </summary>
        public List<string> PublicDependencies = new List<string>();

        /// <summary>
        /// The collection of the modules that are required by this module (for linking).
        /// </summary>
        public List<string> PrivateDependencies = new List<string>();
    }
}
