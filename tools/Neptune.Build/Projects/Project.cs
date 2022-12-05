using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class Project
    {
        private string _path;

        public ProjectGenerator Generator;
        public string Name;
        public Target[] Targets;
        public string WorkspaceRootPath;

        /// <summary>
        /// The source code build defines.
        /// </summary>
        public HashSet<string> Defines = new HashSet<string>();

        /// <summary>
        /// The additional included source files path.
        /// </summary>
        public string[] SearchPaths;

        public virtual string Path
        {
            get => _path;
            set => _path = value;
        }
        public List<string> SourceDirectories;
        public List<string> SourceFiles;

        public string SourceFolderPath => SourceDirectories != null && SourceDirectories.Count > 0 ? SourceDirectories[0] : WorkspaceRootPath;

        /// <summary>
        /// The project output type. Overrides the output type of the target.
        /// </summary>
        public TargetOutputType? OutputType;

        /// <summary>
        /// The configuration data. eg. Windows.Debug | Windows.Release
        /// </summary>
        public struct ConfigurationData
        {
            public string Name;
            public string Text;
            public TargetPlatform Platform;
            public string PlatformName;
            public TargetArchitecture Architecture;
            public TargetConfiguration Configuration;
            public Target Target;
            public Dictionary<Module, BuildOptions> Modules;
            public BuildOptions TargetBuildOptions;
        }
        public List<ConfigurationData> Configurations = new List<ConfigurationData>();

        public virtual void Generate()
        {
            Generator.GenerateProject(this);
        }
    }
}
