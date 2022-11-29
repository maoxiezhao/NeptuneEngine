using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class BuildOptions
    {
        public Target Target;
        public Platform Platform;
        public TargetArchitecture Architecture;
        public TargetConfiguration Configuration;
        public ToolChain Toolchain;
        public List<string> SourcePaths = new List<string>();
        public List<string> SourceFiles = new List<string>();
        public string IntermediateFolder;
        public string OutputFolder;
        public HashSet<string> Libraries = new HashSet<string>();

        /// <summary>
        /// The collection of defines with preprocessing symbol for a source files of this module. Inherited by the modules that include it.
        /// </summary>
        public readonly HashSet<string> PublicDefinitions = new HashSet<string>();

        /// <summary>
        /// The collection of defines with preprocessing symbol for a source files of this module.
        /// </summary>
        public readonly HashSet<string> PrivateDefinitions = new HashSet<string>();

        /// <summary>
        /// The collection of the modules that are required by this module (for linking). Inherited by the modules that include it.
        /// </summary>
        public List<string> PublicDependencies = new List<string>();

        /// <summary>
        /// The collection of the modules that are required by this module (for linking).
        /// </summary>
        public List<string> PrivateDependencies = new List<string>();

        /// <summary>
        /// The build commands working folder directory.
        /// </summary>
        public string WorkingDirectory;

        internal void FillSourceFilesFromSourcePaths()
        {
            if (SourcePaths.Count == 0)
                return;

            for (var i = 0; i < SourcePaths.Count; i++)
            {
                var path = SourcePaths[i];
                if (!Directory.Exists(path))
                    continue;

                var files = Directory.GetFiles(path, "*", SearchOption.AllDirectories);
                if (files.Length <= 0)
                    continue;

                var count = SourceFiles.Count;
                if (SourceFiles.Count == 0)
                {
                    SourceFiles.AddRange(files);
                }
                else
                {
                    for (int j = 0; j < files.Length; j++)
                    {
                        bool unique = true;
                        for (int k = 0; k < count; k++)
                        {
                            if (SourceFiles[k] == files[j])
                            {
                                unique = false;
                                break;
                            }
                        }
                        if (unique)
                            SourceFiles.Add(files[j]);
                    }
                }
            }

            SourcePaths.Clear();
        }
    }
}
