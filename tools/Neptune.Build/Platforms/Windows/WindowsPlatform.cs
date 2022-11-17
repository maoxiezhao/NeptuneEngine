using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Neptune.Build
{
    public class WindowsPlatform : Platform
    {
        public override TargetPlatform Target => TargetPlatform.Windows;
    }
}