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

        public virtual void Setup(BuildOptions options)
        {
        }
    }
}
