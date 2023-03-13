using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Neptune.Build
{
    public enum WindowsPlatformToolset
    {
        /// <summary>
        /// Use the latest toolset.
        /// </summary>
        Latest = 0,

        /// <summary>
        /// Visual Studio 2019
        /// </summary>
        v142 = 142,

        /// <summary>
        /// Visual Studio 2022
        /// </summary>
        v143 = 143,
    }

    public class WindowsPlatform : Platform
    {
        public override TargetPlatform Target => TargetPlatform.Windows;

        protected bool _hasRequiredSDKsInstalled;
        public override bool HasRequiredSDKsInstalled
        {
            get
            {
                return _hasRequiredSDKsInstalled;
            }
        }

        public override string ExecutableFileExtension => ".exe";
        public override string SharedLibraryFileExtension => ".dll";
        public override string StaticLibraryFileExtension => ".lib";
        public override string ProgramDatabaseFileExtension => ".pdb";

        public override bool HasExecutableFileReferenceSupport => true;

        protected override ToolChain CreateToolchain(TargetArchitecture architecture)
        {
            return new WindowsToolchain(this, architecture);
        }

        private static Dictionary<WindowsPlatformToolset, string> _toolsets;
        public static Dictionary<WindowsPlatformToolset, string> GetToolsets()
        {
            if (_toolsets != null)
                return _toolsets;
            _toolsets = new Dictionary<WindowsPlatformToolset, string>();

            // Find toolsets
            var vsInstances = VSInstance.GetInstances();
            foreach (var vs in vsInstances)
            {
                var rootDir = Path.Combine(vs.Path, "VC", "Tools", "MSVC");
                if (!Directory.Exists(rootDir))
                    continue;

                var toolsets = Directory.GetDirectories(rootDir);
                foreach (var toolset in toolsets)
                {
                    if (Version.TryParse(Path.GetFileName(toolset), out var version) &&
                        (File.Exists(Path.Combine(toolset, "bin", "Hostx64", "x64", "cl.exe")) ||
                         File.Exists(Path.Combine(toolset, "bin", "Hostx86", "x64", "cl.exe"))))
                    {
                        if (version.Major == 14 && version.Minor / 10 == 2)
                            _toolsets[WindowsPlatformToolset.v142] = toolset;
                        else if (version.Major == 14 && version.Minor / 10 == 3)
                            _toolsets[WindowsPlatformToolset.v143] = toolset;
                    }
                }
            }

            foreach (var e in _toolsets)
                Log.Info(string.Format("Found Windows toolset {0} at {1}", e.Key, e.Value));

            return _toolsets;
        }   

        /// <summary>
        /// Tries to reads a directory name stored in a registry key.
        /// </summary>
        public static bool TryReadDirRegistryKey(string keyName, string valueName, out string value)
        {
            value = Registry.GetValue(keyName, valueName, null) as string;
            if (string.IsNullOrEmpty(value))
            {
                value = null;
                return false;
            }

            return true;
        }

        /// <summary>
        /// Tries to reads an install directory for a 32-bit program from a registry key. It checks for per-user and machine wide settings, and under the Wow64 virtual keys.
        /// </summary>
        public static bool TryReadInstallDirRegistryKey32(string keySuffix, string valueName, out string dir)
        {
            if (TryReadDirRegistryKey("HKEY_CURRENT_USER\\SOFTWARE\\" + keySuffix, valueName, out dir))
            {
                return true;
            }

            if (TryReadDirRegistryKey("HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\" + keySuffix, valueName, out dir))
            {
                return true;
            }

            if (TryReadDirRegistryKey("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\" + keySuffix, valueName, out dir))
            {
                return true;
            }

            return false;
        }

        private static Dictionary<WindowsPlatformSDK, string> _sdks;
        private static WindowsPlatformSDK[] win10Sdks = new[]
        {
            WindowsPlatformSDK.v10_0_19041_0,
            WindowsPlatformSDK.v10_0_22000_0,
            WindowsPlatformSDK.v10_0_22621_0,
        };

        /// <summary>
        /// Finds all the directories containing the Windows SDKs.
        /// </summary>
        public static Dictionary<WindowsPlatformSDK, string> GetSDKs()
        {
            if (_sdks != null)
                return _sdks;
            _sdks = new Dictionary<WindowsPlatformSDK, string>();

            // Check Windows 10 SDKs
            var sdk10Roots = new HashSet<string>();
            {
                string rootDir;
                if (TryReadInstallDirRegistryKey32("Microsoft\\Windows Kits\\Installed Roots", "KitsRoot10", out rootDir))
                {
                    sdk10Roots.Add(rootDir);
                }

                if (TryReadInstallDirRegistryKey32("Microsoft\\Microsoft SDKs\\Windows\\v10.0", "InstallationFolder", out rootDir))
                {
                    sdk10Roots.Add(rootDir);
                }
            }

            foreach (var sdk10 in sdk10Roots)
            {
                var includeRootDir = Path.Combine(sdk10, "Include");
                if (Directory.Exists(includeRootDir))
                {
                    foreach (var includeDir in Directory.GetDirectories(includeRootDir))
                    {
                        if (!Version.TryParse(Path.GetFileName(includeDir), out var version) || !File.Exists(Path.Combine(includeDir, "um", "windows.h")))
                        {
                            continue;
                        }

                        bool unknown = true;
                        foreach (var win10Sdk in win10Sdks)
                        {
                            if (version == GetSDKVersion(win10Sdk))
                            {
                                _sdks.Add(win10Sdk, sdk10);
                                unknown = false;
                                break;
                            }
                        }
                        if (unknown)
                            Log.Error(string.Format("Unknown Windows SDK version {0} at {1}", version, sdk10));
                    }
                }
            }

            foreach (var e in _sdks)
            {
                Log.Info(string.Format("Found Windows SDK {0} at {1}", e.Key, e.Value));
            }

            return _sdks;
        }

        public WindowsPlatform()
        {
            var sdsk = GetSDKs();
            var toolsets = GetToolsets();
            _hasRequiredSDKsInstalled = sdsk.Count > 0 && toolsets.Count > 0;
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
            case WindowsPlatformSDK.v10_0_19041_0: return new Version(10, 0, 19041, 0);
            case WindowsPlatformSDK.v10_0_22000_0: return new Version(10, 0, 22000, 0);
            case WindowsPlatformSDK.v10_0_22621_0: return new Version(10, 0, 22621, 0);
            default: throw new ArgumentOutOfRangeException(nameof(sdk), sdk, null);
            }
        }
    }
}