using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class Module
    {
        public string Name;
        public string FilePath;
        public string FolderPath;
        public string BinaryModuleName;

        public Module()
        {
            var type = GetType();
            Name = BinaryModuleName = type.Name;
        }

        public virtual void Init()
        {
        }

        /// <summary>
        /// Setups the module build options
        /// </summary>
        public virtual void Setup(BuildOptions options)
        {
        }

        /// <summary>
        /// Setups the module building environment. Allows to modify compiler and linker options.
        /// </summary>
        public virtual void SetupEnvironment(BuildOptions options)
        { 
        }
    }
}
