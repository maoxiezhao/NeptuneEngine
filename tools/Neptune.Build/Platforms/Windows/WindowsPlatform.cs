using Neptune.Build.Platforms;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Neptune.Build
{
    public class WindowsPlatform : Platform
    {
        public override TargetPlatform Target => TargetPlatform.Windows;

        public override bool HasRequiredSDKsInstalled
        {
            get
            {
                return false;
            }
        }

        protected override ToolChain CreateToolchain(TargetArchitecture architecture)
        {
            return null;
        }

        private static Dictionary<WindowsPlatformToolset, string> _toolsets;
        private static Dictionary<WindowsPlatformSDK, string> _sdks;
        public static Dictionary<WindowsPlatformToolset, string> GetToolsets()
        {
            if (_toolsets != null)
                return _toolsets;
            _toolsets = new Dictionary<WindowsPlatformToolset, string>();

            return _toolsets;
        }

        public static Dictionary<WindowsPlatformSDK, string> GetSDKs()
        {
            if (_sdks != null)
                return _sdks;
            _sdks = new Dictionary<WindowsPlatformSDK, string>();

            return _sdks;
        }

        public static string GetVCToolPath32(WindowsPlatformToolset toolset)
        {
            var toolsets = GetToolsets();
            var vcToolChainDir = toolsets[toolset];

            switch (toolset)
            {
            case WindowsPlatformToolset.v142:
            case WindowsPlatformToolset.v143:                 
            {
                string nativeCompilerPath = Path.Combine(vcToolChainDir, "bin", "HostX86", "x86", "cl.exe");
                if (File.Exists(nativeCompilerPath))
                {
                    return Path.GetDirectoryName(nativeCompilerPath);
                }

                throw new Exception(string.Format("No 32-bit compiler toolchain found in {0}", nativeCompilerPath));
            }
            default: throw new ArgumentOutOfRangeException(nameof(toolset), toolset, null);
            }
        }

        public static string GetVCToolPath64(WindowsPlatformToolset toolset)
        {
            var toolsets = GetToolsets();
            var vcToolChainDir = toolsets[toolset];

            switch (toolset)
            {
            case WindowsPlatformToolset.v142:
            case WindowsPlatformToolset.v143:
            {
                string nativeCompilerPath = Path.Combine(vcToolChainDir, "bin", "HostX64", "x64", "cl.exe");
                if (File.Exists(nativeCompilerPath))
                {
                    return Path.GetDirectoryName(nativeCompilerPath);
                }

                string crossCompilerPath = Path.Combine(vcToolChainDir, "bin", "HostX86", "x64", "cl.exe");
                if (File.Exists(crossCompilerPath))
                {
                    return Path.GetDirectoryName(crossCompilerPath);
                }

                throw new Exception(string.Format("No 32-bit compiler toolchain found in {0}", nativeCompilerPath));
            }
            default: throw new ArgumentOutOfRangeException(nameof(toolset), toolset, null);
            }
        }

        public static Version GetSDKVersion(WindowsPlatformSDK sdk)
        {
            switch (sdk)
            {
            case WindowsPlatformSDK.v10_0_22621_0: 
                return new Version(10, 0, 22621, 0);
            default: throw new ArgumentOutOfRangeException(nameof(sdk), sdk, null);
            }
        }
    }
}