using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public enum ModuleType
    {
        Engine,
        Game
    }

    public class Manifest
    {
        public class ModuleInfo
        {
            public ModuleType ModuleType;
            public string ModuleName;
            public string ModuleDirectory;
            public List<string> SourcePaths;
            public List<string> HeaderFiles;
            public List<string> PublicDefines;
            public List<string> IncludePaths;
            public string GeneratedCodeDirectory;
        }

        public string TargetName { get; set; } = string.Empty;
        public List<ModuleInfo> Modules { get; set; } = new List<ModuleInfo>();
    }
}
