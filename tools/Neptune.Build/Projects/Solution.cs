using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class Solution
    {
        public string Name;
        public string Path;
        public string WorkspaceRootPath;
        public Project[] Projects;
        public Project StartupProject;
    }
}
