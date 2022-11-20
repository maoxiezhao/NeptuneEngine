using System;
using System.Collections.Generic;
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

            return modules.GroupBy(x => x.BinaryModuleName).ToArray();
        }
    }
}
