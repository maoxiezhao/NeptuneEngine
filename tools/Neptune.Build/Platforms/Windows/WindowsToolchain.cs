using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Neptune.Build
{ 
    public enum WindowsPlatformSDK
    {
        /// <summary>
        /// Use the latest SDK.
        /// </summary>
        Latest,

        /// <summary>
        /// Windows 10 SDK (10.0.19041.0)
        /// </summary>
        v10_0_19041_0,

        /// <summary>
        /// Windows 11 SDK (10.0.22621.0) 22H2
        /// </summary>
        v10_0_22621_0,
    }

    public sealed class WindowsToolchain : ToolChain
    {
        private readonly string _vcToolPath;
        private readonly string _compilerPath;
        private readonly string _linkerPath;
        private readonly string _libToolPath;
        private readonly string _xdcmakePath;

        public WindowsPlatformToolset Toolset { get; }
        public WindowsPlatformSDK SDK { get; }

        public override string DllExport => "__declspec(dllexport)";
        public override string DllImport => "__declspec(dllimport)";

        public WindowsToolchain(WindowsPlatform platform, TargetArchitecture architecture) :
            base(platform, architecture)
        {
            var toolsetVer = WindowsPlatformToolset.Latest;
            var sdkVer = WindowsPlatformSDK.Latest;

            var toolsets = WindowsPlatform.GetToolsets();
            var sdks = WindowsPlatform.GetSDKs();

            // Pick the latest toolset
            if (toolsetVer == WindowsPlatformToolset.Latest)
            {
                toolsetVer = toolsets.Keys.Max();
            }
            Toolset = toolsetVer;

            // Pick the latest SDK
            if (sdkVer == WindowsPlatformSDK.Latest)
            {
                sdkVer = sdks.Keys.Max();
            }
            SDK = sdkVer;

            // Get the tools paths
            string vcToolPath;
            if (Architecture == TargetArchitecture.x64)
                vcToolPath = WindowsPlatform.GetVCToolPath64(Toolset);
            else
                vcToolPath = WindowsPlatform.GetVCToolPath32(Toolset);
            _vcToolPath = vcToolPath;
            _compilerPath = Path.Combine(vcToolPath, "cl.exe");
            _linkerPath = Path.Combine(vcToolPath, "link.exe");
            _libToolPath = Path.Combine(vcToolPath, "lib.exe");
            _xdcmakePath = Path.Combine(vcToolPath, "xdcmake.exe");

            // Add Visual C++ toolset include and library paths
            var vcToolChainDir = toolsets[Toolset];
            SystemIncludePaths.Add(Path.Combine(vcToolChainDir, "include"));
            switch (Toolset)
            {
            case WindowsPlatformToolset.v143:
            {
                switch (Architecture)
                {
                case TargetArchitecture.x86:
                    SystemLibraryPaths.Add(Path.Combine(vcToolChainDir, "lib", "x86"));
                    break;
                case TargetArchitecture.x64:
                    SystemLibraryPaths.Add(Path.Combine(vcToolChainDir, "lib", "x64"));
                    break;
                default: throw new ArgumentOutOfRangeException();
                }
                break;
            }
            default: throw new ArgumentOutOfRangeException();
            }

            // Add Windows SDK include and library paths
            var windowsSdkDir = sdks[SDK];
            switch (SDK)
            {
            case WindowsPlatformSDK.v10_0_19041_0:
            case WindowsPlatformSDK.v10_0_22621_0:
            {
                var sdkVersionName = WindowsPlatform.GetSDKVersion(SDK).ToString();
                string includeRootDir = Path.Combine(windowsSdkDir, "include", sdkVersionName);
                SystemIncludePaths.Add(Path.Combine(includeRootDir, "ucrt"));
                SystemIncludePaths.Add(Path.Combine(includeRootDir, "shared"));
                SystemIncludePaths.Add(Path.Combine(includeRootDir, "um"));
                SystemIncludePaths.Add(Path.Combine(includeRootDir, "winrt"));

                string libraryRootDir = Path.Combine(windowsSdkDir, "lib", sdkVersionName);
                switch (Architecture)
                {
                case TargetArchitecture.x86:
                    SystemLibraryPaths.Add(Path.Combine(libraryRootDir, "ucrt", "x86"));
                    SystemLibraryPaths.Add(Path.Combine(libraryRootDir, "um", "x86"));
                    break;
                case TargetArchitecture.x64:
                    SystemLibraryPaths.Add(Path.Combine(libraryRootDir, "ucrt", "x64"));
                    SystemLibraryPaths.Add(Path.Combine(libraryRootDir, "um", "x64"));
                    break;
                default: throw new ArgumentOutOfRangeException();
                }
                break;
            }
            default: throw new ArgumentOutOfRangeException(nameof(SDK));
            }
        }

        public override CompileOutput CompileCppFiles(TaskGraph graph, BuildOptions options, List<string> sourceFiles, string outputPath)
        {
            var output = new CompileOutput();

            return output;
        }
    }
}