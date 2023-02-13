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

        /// <summary>
        /// Gets the executable file extension (including leading dot).
        /// </summary>
        public abstract string ExecutableFileExtension { get; }

        /// <summary>
        /// Gets the shared library file extension (including leading dot).
        /// </summary>
        public abstract string SharedLibraryFileExtension { get; }

        /// <summary>
        /// Gets the static library file extension (including leading dot).
        /// </summary>
        public abstract string StaticLibraryFileExtension { get; }

        /// <summary>
        /// Gets the program database file extension (including leading dot).
        /// </summary>
        public abstract string ProgramDatabaseFileExtension { get; }

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

        /// <summary>
        /// Tries to create the build toolchain for a given architecture. Returns null if platform is not supported.
        /// </summary>
        public ToolChain TryGetToolChain(TargetArchitecture targetArchitecture)
        {
            return HasRequiredSDKsInstalled ? GetToolchain(targetArchitecture) : null;
        }

        private Dictionary<TargetArchitecture, ToolChain> _toolchains;
        public ToolChain GetToolchain(TargetArchitecture targetArchitecture)
        {
            if (_toolchains == null)
                _toolchains = new Dictionary<TargetArchitecture, ToolChain>();

            // Get caching toolchain
            ToolChain toolchain;
            if (_toolchains.TryGetValue(targetArchitecture, out toolchain))
            {
                return toolchain;
            }

            // Create new toolchain
            toolchain = CreateToolchain(targetArchitecture);
            _toolchains.Add(targetArchitecture, toolchain);
            return toolchain;
        }

        protected abstract ToolChain CreateToolchain(TargetArchitecture architecture);

        public string GetLinkOutputFileName(string name, LinkerOutput output)
        {
            switch (output)
            {
                case LinkerOutput.Executable: 
                    return name + ExecutableFileExtension;
                case LinkerOutput.SharedLibrary: 
                    return name + SharedLibraryFileExtension;
                case LinkerOutput.StaticLibrary:
                    return name + StaticLibraryFileExtension;
                default: throw new ArgumentOutOfRangeException(nameof(output), output, null);
            }
        }
    }
}
