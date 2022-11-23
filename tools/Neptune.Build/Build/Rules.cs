using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class RulesAssembly
    {
        public readonly Assembly Assembly;
        public readonly Target[] Targets;
        public readonly Module[] Modules;

        private readonly Dictionary<string, Module> _modulesLookup;

        internal RulesAssembly(Assembly assembly, Target[] targets, Module[] modules)
        {
            Assembly = assembly;
            Targets = targets;
            Modules = modules;

            _modulesLookup = new Dictionary<string, Module>(modules.Length);
            for (int i = 0; i < modules.Length; i++)
            {
                var module = modules[i];
                _modulesLookup.Add(module.Name, module);
            }
        }

        public Module GetModule(string name)
        {
            if (!_modulesLookup.TryGetValue(name, out var module))
                module = Modules.FirstOrDefault(x => string.Equals(x.Name, name, StringComparison.OrdinalIgnoreCase));
            return module;
        }
    }
}
