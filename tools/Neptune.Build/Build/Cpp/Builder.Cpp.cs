using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    partial class Builder
    {
        private static IGrouping<string, Module>[] GetBinaryModules(ProjectInfo project, Target target, Dictionary<Module, BuildOptions> buildModules)
        {
            var modules = new List<Module>();
            switch (target.LinkType)
            {
            case TargetLinkType.Static:
                modules.AddRange(buildModules.Keys);
                break;
            case TargetLinkType.Dynamic:
            {
                var sourcePath = Path.Combine(project.ProjectFolderPath, "Source");
                foreach (var module in buildModules.Keys)
                {
                    if (module.FolderPath.StartsWith(sourcePath))
                        modules.Add(module);
                }
                break;
            }
            default: throw new ArgumentOutOfRangeException();
            }
            modules.RemoveAll(x => x == null || string.IsNullOrEmpty(x.BinaryModuleName));
            return modules.GroupBy(x => x.BinaryModuleName).ToArray();
        }
    }
}
