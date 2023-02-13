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
        public string GroupName = string.Empty;
        public Target[] Targets;
        public string WorkspaceRootPath;
        public Guid FolderGuid;

        /// <summary>
        /// The source code build defines.
        /// </summary>
        public HashSet<string> Defines = new HashSet<string>();

        /// <summary>
        /// The additional included source files path.
        /// </summary>
        public string[] SearchPaths;

        /// <summary>
        /// The project dependencies.
        /// </summary>
        public HashSet<Project> Dependencies = new HashSet<Project>();

        public TargetType TargetType = TargetType.Cpp;

        /// <summary>
        /// The native C# project options.
        /// </summary>
        public CSharpProject CSharp = new CSharpProject
        {
            SystemReferences = new HashSet<string>(),
            FileReferences = new HashSet<string>(),
        };

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
