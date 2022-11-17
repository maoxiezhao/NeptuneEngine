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
        public virtual string Path
        {
            get => _path;
            set => _path = value;
        }
        public List<string> SourceDirectories;

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
        }
        public List<ConfigurationData> Configurations = new List<ConfigurationData>();
    }
}
