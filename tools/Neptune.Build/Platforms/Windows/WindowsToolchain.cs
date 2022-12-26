using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

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
            var compileEnvironment = options.CompileEnv;
            var output = new CompileOutput();

            // Setup common arguments
            var commonArgs = new List<string>();
            {
                commonArgs.Add("/nologo");

                // Compile Without Linking
                commonArgs.Add("/c");

                // C++ version
                switch (compileEnvironment.CppVersion)
                {
                case CppVersion.Cpp17:
                    commonArgs.Add("/std:c++17");
                    break;
                case CppVersion.Cpp20:
                    commonArgs.Add("/std:c++20");
                    break;
                case CppVersion.Latest:
                    commonArgs.Add("/std:c++latest");
                    break;
                }

                // Generate Intrinsic Functions
                if (compileEnvironment.IntrinsicFunctions)
                    commonArgs.Add("/Oi");

                // Enable Function-Level Linking
                if (compileEnvironment.FunctionLevelLinking)
                    commonArgs.Add("/Gy");
                else
                    commonArgs.Add("/Gy-");

                // Code Analysis
                commonArgs.Add("/analyze-");

                // Remove unreferenced COMDAT
                commonArgs.Add("/Zc:inline");

                if (compileEnvironment.OptimizationHit == OptimizationHit.FastCode)
                    commonArgs.Add("/Ot");
                else if (compileEnvironment.OptimizationHit == OptimizationHit.SmallCode)
                    commonArgs.Add("/Os");

                // Run-Time Error Checks
                if (compileEnvironment.RuntimeChecks)
                    commonArgs.Add("/RTC1");

                // Inline Function Expansion
                if (compileEnvironment.Inlining)
                    commonArgs.Add("/Ob2");

                if (compileEnvironment.DebugInformation)
                {
                    // Debug Information Format
                    commonArgs.Add("/Zi");

                    // Enhance Optimized Debugging
                    commonArgs.Add("/Zo");
                }

                if (compileEnvironment.Optimization)
                {
                    // Enable Most Speed Optimizations
                    commonArgs.Add("/Ox");

                    // Generate Intrinsic Functions
                    commonArgs.Add("/Oi");

                    // Frame-Pointer Omission
                    commonArgs.Add("/Oy");

                    if (compileEnvironment.WholeProgramOptimization)
                    {
                        // Whole Program Optimization
                        commonArgs.Add("/GL");
                    }
                }
                else
                {
                    // Disable compiler optimizations (Debug)
                    commonArgs.Add("/Od");

                    // Frame-Pointer Omission
                    commonArgs.Add("/Oy-");
                }

                // Full Path of Source Code File in Diagnostics
                commonArgs.Add("/FC");

                // Report Internal Compiler Errors
                commonArgs.Add("/errorReport:prompt");

                // Disable Exceptions
                commonArgs.Add("/D_HAS_EXCEPTIONS=0");

                // Eliminate Duplicate Strings
                if (compileEnvironment.StringPooling)
                    commonArgs.Add("/GF");
                else
                    commonArgs.Add("/GF-");

                if (compileEnvironment.UseDebugCRT)
                    commonArgs.Add("/MDd");
                else
                    commonArgs.Add("/MD");

                // Specify floating-point behavior
                commonArgs.Add("/fp:fast");
                commonArgs.Add("/fp:except-");

                // Buffer Security Check
                if (compileEnvironment.BufferSecurityCheck)
                    commonArgs.Add("/GS");
                else
                    commonArgs.Add("/GS-");

                // Enable Run-Time Type Information
                commonArgs.Add("/GR");

                // Dont treat all compiler warnings as errors
                commonArgs.Add("/WX-");

                // Show warnings
                commonArgs.Add("/W3");
                commonArgs.Add("/wd\"4005\"");

                // wchar_t is Native Type
                commonArgs.Add("/Zc:wchar_t");
            }

            // Add preprocessor definitions
            foreach (var definition in compileEnvironment.PreprocessorDefinitions)
            {
                commonArgs.Add(string.Format("/D \"{0}\"", definition));
            }

            // Add include paths
            foreach (var includePath in compileEnvironment.IncludePaths)
            {
                if (includePath.Contains(' '))
                    commonArgs.Add(string.Format("/I\"{0}\"", includePath));
                else
                    commonArgs.Add(string.Format("/I{0}", includePath));
            }

            var args = new List<string>();
            foreach (var sourceFile in sourceFiles)
            {
                var sourceFilename = Path.GetFileNameWithoutExtension(sourceFile);
                var task = graph.Add<CmdTask>();

                args.Clear();
                args.AddRange(commonArgs);

                if (compileEnvironment.DebugInformation)
                {
                    var pdbFile = Path.Combine(outputPath, sourceFilename + ".pdb");
                    args.Add(string.Format("/Fd\"{0}\"", pdbFile));
                    output.DebugDataFiles.Add(pdbFile);
                }

                // Obj File Name
                var objFile = Path.Combine(outputPath, sourceFilename + ".obj");
                args.Add(string.Format("/Fo\"{0}\"", objFile));
                output.ObjectFiles.Add(objFile);
                task.ProducedFiles.Add(objFile);

                // Source File Name
                args.Add("\"" + sourceFile + "\"");

                // Request included files
                var includes = IncludesCache.FindAllIncludedFiles(sourceFile);
                task.PrerequisiteFiles.AddRange(includes);

                // Setup compiling task
                task.WorkingDirectory = options.WorkingDirectory;
                task.CommandPath = _compilerPath;
                task.CommandArguments = string.Join(" ", args);
                task.PrerequisiteFiles.Add(sourceFile);
                task.Cost = task.PrerequisiteFiles.Count;
            }

            return output;
        }

        /// <summary>
        /// Links the compiled object files.
        /// </summary>
        public override void LinkFiles(TaskGraph graph, BuildOptions options, string outputFilePath)
        {
            // https://learn.microsoft.com/zh-cn/cpp/build/reference/linking?view=msvc-170

            var linkEnvironment = options.LinkEnv;
            var task = graph.Add<CmdTask>();
            var args = new List<string>();
            {
                // Suppress startup banner
                args.Add("/NOLOGO");

                // Report internal compiler errors
                args.Add("/ERRORREPORT:PROMPT");

                // Output File Name
                args.Add(string.Format("/OUT:\"{0}\"", outputFilePath));

                // Specify target platform
                switch (Architecture)
                {
                    case TargetArchitecture.x86:
                        args.Add("/MACHINE:x86");
                        break;
                    case TargetArchitecture.x64:
                        args.Add("/MACHINE:x64");
                        break;
                    default: throw new Exception();
                }

                // Windows system
                if (linkEnvironment.LinkAsConsoleProgram)
                {
                    args.Add("/SUBSYSTEM:CONSOLE");
                }
                else
                {
                    args.Add("/SUBSYSTEM:WINDOWS");
                }

                // Link-time code generation
                if (linkEnvironment.LinkTimeCodeGeneration)
                {
                    args.Add("/LTCG");
                }

                // Don't create Side-by-Side Assembly Manifest
                args.Add("/MANIFEST:NO");

                // Fixed Base Address
                args.Add("/FIXED:NO");

                if (Architecture == TargetArchitecture.x86)
                {
                    // Handle Large Addresses
                    args.Add("/LARGEADDRESSAWARE");
                }

                // Compatible with Data Execution Prevention
                args.Add("/NXCOMPAT");

                // Allow delay-loaded DLLs to be explicitly unloaded
                args.Add("/DELAY:UNLOAD");

                if (linkEnvironment.Output == LinkerOutput.SharedLibrary)
                {
                    // Build a DLL
                    args.Add("/DLL");
                }

                // Redirect imports LIB file auto-generated for EXE/DLL
                var libFile = Path.ChangeExtension(outputFilePath, Platform.StaticLibraryFileExtension);
                args.Add("/IMPLIB:\"" + libFile + "\"");
                task.ProducedFiles.Add(libFile);

                // Don't embed the full PDB path
                args.Add("/PDBALTPATH:%_PDB%");

                // Optimize
                if (linkEnvironment.Optimization && !linkEnvironment.UseIncrementalLinking)
                {
                    // Generate an EXE checksum
                    args.Add("/RELEASE");

                    // Eliminate unreferenced symbols
                    args.Add("/OPT:REF");

                    // Remove redundant COMDATs
                    args.Add("/OPT:ICF");
                }
                else
                {
                    // Keep symbols that are unreferenced
                    args.Add("/OPT:NOREF");

                    // Disable identical COMDAT folding
                    args.Add("/OPT:NOICF");
                }

                // Link Incrementally
                if (linkEnvironment.UseIncrementalLinking)
                {
                    args.Add("/INCREMENTAL");
                }
                else
                {
                    args.Add("/INCREMENTAL:NO");
                }

                if (linkEnvironment.DebugInformation)
                {
                    args.Add("/DEBUG");

                    // Use Program Database
                    var pdbFile = Path.ChangeExtension(outputFilePath, Platform.ProgramDatabaseFileExtension);
                    args.Add(string.Format("/PDB:\"{0}\"", pdbFile));
                    task.ProducedFiles.Add(pdbFile);
                }
            }

            // Delay-load DLLs
            if (linkEnvironment.Output == LinkerOutput.Executable || linkEnvironment.Output == LinkerOutput.SharedLibrary)
            {
                foreach (var dll in options.DelayLoadLibraries)
                {
                    args.Add(string.Format("/DELAYLOAD:\"{0}\"", dll));
                }
            }

            // Additional lib paths
            foreach (var libpath in linkEnvironment.LibraryPaths)
            {
                args.Add(string.Format("/LIBPATH:\"{0}\"", libpath));
            }

            // Input libraries
            task.PrerequisiteFiles.AddRange(linkEnvironment.InputLibraries);
            foreach (var library in linkEnvironment.InputLibraries)
            {
                args.Add(string.Format("\"{0}\"", library));
            }

            // Input files
            task.PrerequisiteFiles.AddRange(linkEnvironment.InputFiles);
            foreach (var file in linkEnvironment.InputFiles)
            {
                args.Add(string.Format("\"{0}\"", file));
            }

            // Response file
            var responseFile = Path.Combine(options.IntermediateFolder, Path.GetFileName(outputFilePath) + ".response");
            task.PrerequisiteFiles.Add(responseFile);
            Utils.WriteFileIfChanged(responseFile, string.Join(Environment.NewLine, args));

            // Setup task
            task.WorkingDirectory = options.WorkingDirectory;
            task.CommandPath = _linkerPath;
            task.CommandArguments = string.Format("@\"{0}\"", responseFile);
            task.InfoMessage = "Linking " + outputFilePath;
            task.Cost = task.PrerequisiteFiles.Count;
            task.ProducedFiles.Add(outputFilePath);
        }

        public override void CreateImportLib(TaskGraph graph, BuildOptions options, string outputFilePath)
        {
            // https://learn.microsoft.com/zh-cn/cpp/build/reference/overview-of-lib?view=msvc-170
            var linkEnvironment = options.LinkEnv;
            if (linkEnvironment.Output != LinkerOutput.ImportLibrary)
                return;

            var task = graph.Add<CmdTask>();
            var args = new List<string>();
            {
                // Suppress startup banner
                args.Add("/NOLOGO");

                // Report internal compiler errors
                args.Add("/ERRORREPORT:PROMPT");

                // Output File Name
                args.Add(string.Format("/OUT:\"{0}\"", outputFilePath));

                // Specify target platform
                switch (Architecture)
                {
                    case TargetArchitecture.x86:
                        args.Add("/MACHINE:x86");
                        break;
                    case TargetArchitecture.x64:
                        args.Add("/MACHINE:x64");
                        break;
                    default: throw new Exception();
                }

                // Windows system
                args.Add("/SUBSYSTEM:WINDOWS");

                // Link-time code generation
                if (linkEnvironment.LinkTimeCodeGeneration)
                {
                    args.Add("/LTCG");
                }

                // Create an import library
                args.Add("/DEF");

                // Ignore libraries
                args.Add("/NODEFAULTLIB");

                // Specify the name
                args.Add(string.Format("/NAME:\"{0}\"", Path.GetFileNameWithoutExtension(outputFilePath)));

                // Ignore warnings about files with no public symbols
                args.Add("/IGNORE:4221");
            }

            // Delay-load DLLs
            if (linkEnvironment.Output == LinkerOutput.Executable || linkEnvironment.Output == LinkerOutput.SharedLibrary)
            {
                foreach (var dll in options.DelayLoadLibraries)
                {
                    args.Add(string.Format("/DELAYLOAD:\"{0}\"", dll));
                }
            }

            // Additional lib paths
            foreach (var libpath in linkEnvironment.LibraryPaths)
            {
                args.Add(string.Format("/LIBPATH:\"{0}\"", libpath));
            }

            // Input libraries
            task.PrerequisiteFiles.AddRange(linkEnvironment.InputLibraries);
            foreach (var library in linkEnvironment.InputLibraries)
            {
                args.Add(string.Format("\"{0}\"", library));
            }

            // Input files
            task.PrerequisiteFiles.AddRange(linkEnvironment.InputFiles);
            foreach (var file in linkEnvironment.InputFiles)
            {
                args.Add(string.Format("\"{0}\"", file));
            }

            // Response file
            var responseFile = Path.Combine(options.IntermediateFolder, Path.GetFileName(outputFilePath) + ".response");
            task.PrerequisiteFiles.Add(responseFile);
            Utils.WriteFileIfChanged(responseFile, string.Join(Environment.NewLine, args));

            // Setup task
            task.WorkingDirectory = options.WorkingDirectory;
            task.CommandPath = _libToolPath;
            task.CommandArguments = string.Format("@\"{0}\"", responseFile);
            task.InfoMessage = "Building import library " + outputFilePath;
            task.Cost = task.PrerequisiteFiles.Count;
            task.ProducedFiles.Add(outputFilePath);
        }
    }
}