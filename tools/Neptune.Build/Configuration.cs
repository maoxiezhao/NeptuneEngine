﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public static partial class Configuration
    {
        /// <summary>
        /// The intermediate build files folder path relative to the working directory.
        /// </summary>
        [CommandLine("intermediate", "<path>")]
        public static string IntermediateFolder = "Cache/Intermediate";

        /// <summary>
        /// The output binaries folder path relative to the working directory.
        /// </summary>
        [CommandLine("binaries", "<path>", "The output binaries folder path relative to the working directory.")]
        public static string BinariesFolder = "Bin";

        /// <summary>
        /// Cleans all the targets and whole build cache data.
        /// </summary>
        [CommandLine("clean", "Cleans the build system cache.")]
        public static bool Clean = false;

        /// <summary>
        /// Generates the projects for the workspace.
        /// </summary>
        [CommandLine("genproject", "Generates the projects for the workspace.")]
        public static bool GenerateProject = false;

        /// <summary>
        /// Builds the targets. Builds all the targets, use <see cref="BuildTargets"/> to select a custom set of targets for the build.
        /// </summary>
        [CommandLine("build", "Builds the targets.")]
        public static bool Build = false;

        /// <summary>
        /// Generates Visual Studio 2022 project format files. Valid only with -genproject option.
        /// </summary>
        [CommandLine("vs2022", "Generates Visual Studio 2022 project.")]
        public static bool ProjectVS2022 = false;
    }
}