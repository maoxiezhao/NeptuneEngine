using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public enum TargetPlatform
    {
        Windows = 1,
        Linux = 2,
    }

    public enum TargetArchitecture
    {
        x64 = 0,
        x86 = 1,
    }

    public enum TargetConfiguration
    {
        Debug = 0,
        Development = 1,
        Release = 2,
    }
}
