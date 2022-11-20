using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class ToolChain
    {
        public Platform Platform { get; private set; }
        public TargetArchitecture Architecture { get; private set; }

        protected ToolChain(Platform platform, TargetArchitecture architecture)
        {
            Platform = platform;
            Architecture = architecture;
        }
    }
}
