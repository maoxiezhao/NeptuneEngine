using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public abstract class Platform
    {
        public abstract TargetPlatform Target { get; }

        private static Platform[] _platforms;

        private static Type[] _buildTypes;
        internal static Type[] BuildTypes
        {
            get
            {
                if (_buildTypes == null)
                {
                    using (new ProfileEventScope("CacheBuildTypes"))
                    {
                        _buildTypes = typeof(Program).Assembly.GetTypes();
                    }
                }
                return _buildTypes;
            }
        }

        /// <summary>
        /// Check whether required external SDKs are installed for this platform.
        /// </summary>
        public abstract bool HasRequiredSDKsInstalled { get; }

        public static Platform GetPlatform(TargetPlatform targetPlatform)
        {
            if (_platforms == null)
                _platforms = BuildTypes.Where(x => x.IsClass && !x.IsAbstract && x.IsSubclassOf(typeof(Platform))).Select(Activator.CreateInstance).Cast<Platform>().ToArray();

            foreach (var platform in _platforms)
            {
                if (platform.Target == targetPlatform)
                {
                    return platform;
                }
            }

            return null;
        }

        public static bool IsPlatformSupported(TargetPlatform targetPlatform, TargetArchitecture targetArchitecture)
        {
            if (targetPlatform == TargetPlatform.Windows)
            {
                return targetArchitecture == TargetArchitecture.x64 || 
                       targetArchitecture == TargetArchitecture.x86;
            }
            return false;
        }
    }
}
