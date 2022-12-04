using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Microsoft.VisualStudio.Setup.Configuration;

namespace Neptune.Build
{
    public sealed class VSInstance
    {
        /// <summary>
        /// The version.
        /// </summary>
        public VisualStudioVersion Version;

        /// <summary>
        /// The install directory path.
        /// </summary>
        public string Path;


        private static List<VSInstance> _installDirs;
        public static IReadOnlyList<VSInstance> GetInstances()
        {
            if (_installDirs != null)
                return _installDirs;

            _installDirs = new List<VSInstance>();

            if (Environment.OSVersion.Platform == PlatformID.Win32NT)
            {
                SetupConfiguration setup = new SetupConfiguration();
                IEnumSetupInstances enumerator = setup.EnumAllInstances();
                ISetupInstance[] instances = new ISetupInstance[1];
                while (true)
                {
                    enumerator.Next(1, instances, out int fetchedCount);
                    if (fetchedCount == 0)
                        break;

                    ISetupInstance2 instance = (ISetupInstance2)instances[0];
                    if ((instance.GetState() & InstanceState.Local) == InstanceState.Local)
                    {
                        VisualStudioVersion version;
                        string displayName = instance.GetDisplayName();
                        if (displayName.Contains("2019"))
                            version = VisualStudioVersion.VS2019;
                        else if (displayName.Contains("2022"))
                            version = VisualStudioVersion.VS2022;
                        else
                        {
                            Log.Warning(string.Format("Unknown Visual Studio installation. Display name: {0}", displayName));
                            continue;
                        }

                        var vsInstance = new VSInstance
                        {
                            Version = version,
                            Path = instance.GetInstallationPath(),
                        };
                        _installDirs.Add(vsInstance);
                    }
                }
            }

            return _installDirs;
        }
    }
}
