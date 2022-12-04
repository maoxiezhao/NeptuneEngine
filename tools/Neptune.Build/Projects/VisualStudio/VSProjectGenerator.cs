using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;
using System.Threading.Tasks;
using static System.Net.WebRequestMethods;

namespace Neptune.Build
{
    public enum VisualStudioVersion
    {
        VS2019,
        VS2022,
    }

    public class VSProjectGenerator : ProjectGenerator
    {
        public override string ProjectFileExtension => "vcxproj";
        public override string SolutionFileExtension => "sln";

        public readonly VisualStudioVersion Version;
        public string ProjectFileToolVersion
        {
            get
            {
                switch (Version)
                {
                    case VisualStudioVersion.VS2019: return "16.0";
                    case VisualStudioVersion.VS2022: return "17.0";
                }

                return string.Empty;
            }
        }

        /// <summary>
        /// Gets the project file platform toolset version.
        /// </summary>
        public string ProjectFilePlatformToolsetVersion
        {
            get
            {
                switch (Version)
                {
                    case VisualStudioVersion.VS2019: return "v142";
                    case VisualStudioVersion.VS2022: return "v143";
                }
                return string.Empty;
            }
        }

        public VSProjectGenerator(VisualStudioVersion version)
        {
            Version = version;
        }

        private static string FixPath(string path)
        {
            if (path.Contains(' '))
            {
                path = "\"" + path + "\"";
            }
            return path;
        }

        private static string GetFileType(string file)
        {
            string fileType;
            if (file.EndsWith(".h", StringComparison.OrdinalIgnoreCase) || file.EndsWith(".inl", StringComparison.OrdinalIgnoreCase))
            {
                fileType = "ClInclude";
            }
            else if (file.EndsWith(".cpp", StringComparison.OrdinalIgnoreCase) || file.EndsWith(".cc", StringComparison.OrdinalIgnoreCase))
            {
                fileType = "ClCompile";
            }
            else if (file.EndsWith(".rc", StringComparison.OrdinalIgnoreCase))
            {
                fileType = "ResourceCompile";
            }
            else if (file.EndsWith(".manifest", StringComparison.OrdinalIgnoreCase))
            {
                fileType = "Manifest";
            }
            else
            {
                fileType = "None";
            }
            return fileType;
        }

        public override void GenerateProject(Project project)
        {
            var vsProject = project as VisualStudioProject;
            if (vsProject == null)
            {
                Log.Error("Unmatched project");
                return;
            }

            var projectDirectory = Path.GetDirectoryName(project.Path);
            var projectFileToolVersion = ProjectFileToolVersion;

            var files = new List<string>();
            if (project.SourceFiles != null)
                files.AddRange(project.SourceFiles);

            if (project.SourceDirectories != null)
            {
                foreach (var folder in project.SourceDirectories)
                {
                    files.AddRange(Directory.GetFiles(folder, "*", SearchOption.AllDirectories));
                }
            }

            var vcProjectFileContent = new StringBuilder();
            var vcFiltersFileContent = new StringBuilder();
            var vcUserFileContent = new StringBuilder();

            // Project file
            {
                vcProjectFileContent.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
                vcProjectFileContent.AppendLine(string.Format("<Project DefaultTargets=\"Build\" ToolsVersion=\"{0}\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">", projectFileToolVersion));

                // Project configurations
                vcProjectFileContent.AppendLine("  <ItemGroup Label=\"ProjectConfigurations\">");
                foreach (var config in project.Configurations)
                {
                    vcProjectFileContent.AppendLine(string.Format("    <ProjectConfiguration Include=\"{0}\">", config.Name));
                    vcProjectFileContent.AppendLine(string.Format("      <Configuration>{0}</Configuration>", config.Text));
                    vcProjectFileContent.AppendLine(string.Format("      <Platform>{0}</Platform>", config.Architecture));
                    vcProjectFileContent.AppendLine("    </ProjectConfiguration>");
                }
                vcProjectFileContent.AppendLine("  </ItemGroup>");

                // Globals
                vcProjectFileContent.AppendLine("  <PropertyGroup Label=\"Globals\">");
                vcProjectFileContent.AppendLine(string.Format("    <ProjectGuid>{0}</ProjectGuid>", vsProject.ProjectGuid.ToString("B").ToUpperInvariant()));
                vcProjectFileContent.AppendLine(string.Format("    <RootNamespace>{0}</RootNamespace>", project.Name));
                vcProjectFileContent.AppendLine(string.Format("    <PlatformToolset>{0}</PlatformToolset>", ProjectFilePlatformToolsetVersion));
                vcProjectFileContent.AppendLine(string.Format("    <MinimumVisualStudioVersion>{0}</MinimumVisualStudioVersion>", projectFileToolVersion));
                vcProjectFileContent.AppendLine("    <TargetRuntime>Native</TargetRuntime>");
                vcProjectFileContent.AppendLine("    <CharacterSet>Unicode</CharacterSet>");
                vcProjectFileContent.AppendLine("    <Keyword>MakeFileProj</Keyword>");
                vcProjectFileContent.AppendLine("  </PropertyGroup>");

                // Default properties
                vcProjectFileContent.AppendLine("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />");

                // Per configuration property sheets
                foreach (var configuration in project.Configurations)
                {
                    vcProjectFileContent.AppendLine(string.Format("  <ImportGroup Condition=\"'$(Configuration)|$(Platform)'=='{0}'\" Label=\"PropertySheets\">", configuration.Name));
                    vcProjectFileContent.AppendLine("    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />");
                    vcProjectFileContent.AppendLine("  </ImportGroup>");
                }

                // Per configuration property sheets
                foreach (var configuration in project.Configurations)
                {
                    vcProjectFileContent.AppendLine(string.Format("  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='{0}'\" Label=\"Configuration\">", configuration.Name));
                    vcProjectFileContent.AppendLine("    <ConfigurationType>Makefile</ConfigurationType>");
                    vcProjectFileContent.AppendLine(string.Format("    <PlatformToolset>{0}</PlatformToolset>", ProjectFilePlatformToolsetVersion));
                    vcProjectFileContent.AppendLine("  </PropertyGroup>");
                }

                vcProjectFileContent.AppendLine("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />");
                vcProjectFileContent.AppendLine("  <ImportGroup Label=\"ExtensionSettings\" />");

                // User macros
                vcProjectFileContent.AppendLine("  <PropertyGroup Label=\"UserMacros\" />");

                // Per configuration options
                var buildToolPath = Utils.MakePathRelativeTo(typeof(Builder).Assembly.Location, projectDirectory);

                var preprocessorDefinitions = new HashSet<string>();
                var includePaths = new HashSet<string>();
                foreach (var configuration in project.Configurations)
                {
                    var targetBuildOptions = configuration.TargetBuildOptions;

                    // Collect definitions
                    preprocessorDefinitions.Clear();
                    foreach (var e in targetBuildOptions.CompileEnv.PreprocessorDefinitions)
                        preprocessorDefinitions.Add(e);

                    // Collect include paths
                    includePaths.Clear();
                    foreach (var e in targetBuildOptions.CompileEnv.IncludePaths)
                        includePaths.Add(e);

                    var cmdLine = string.Format("{0} -log -mutex -workspace={1} -arch={2} -configuration={3} -platform={4} -buildTargets={5}",
                                                FixPath(buildToolPath),
                                                FixPath(project.WorkspaceRootPath),
                                                configuration.Architecture,
                                                configuration.Configuration,
                                                configuration.Platform,
                                                configuration.Target.Name);

                    vcProjectFileContent.AppendLine(string.Format("  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='{0}'\">", configuration.Name));
                    vcProjectFileContent.AppendLine(string.Format("    <IntDir>{0}</IntDir>", targetBuildOptions.IntermediateFolder));
                    vcProjectFileContent.AppendLine(string.Format("    <OutDir>{0}</OutDir>", targetBuildOptions.OutputFolder));
                    vcProjectFileContent.AppendLine(string.Format("    <NMakeBuildCommandLine>{0} -build</NMakeBuildCommandLine>", cmdLine));
                    vcProjectFileContent.AppendLine(string.Format("    <NMakeReBuildCommandLine>{0} -rebuild</NMakeReBuildCommandLine>", cmdLine));
                    vcProjectFileContent.AppendLine(string.Format("    <NMakeCleanCommandLine>{0} -clean</NMakeCleanCommandLine>", cmdLine));
                    var outputTargetFilePath = configuration.Target.GetOutputFilePath(targetBuildOptions, project.OutputType);
                    vcProjectFileContent.AppendLine(string.Format("    <NMakeOutput>{0}</NMakeOutput>", outputTargetFilePath));

                    if (preprocessorDefinitions.Count != 0)
                        vcProjectFileContent.AppendLine(string.Format("    <NMakePreprocessorDefinitions>$(NMakePreprocessorDefinitions);{0}</NMakePreprocessorDefinitions>", string.Join(";", preprocessorDefinitions)));

                    if (includePaths.Count != 0)
                        vcProjectFileContent.AppendLine(string.Format("    <NMakeIncludeSearchPath>$(NMakeIncludeSearchPath);{0}</NMakeIncludeSearchPath>", string.Join(";", includePaths)));


                    vcProjectFileContent.AppendLine("  </PropertyGroup>");
                }

                // Files and folders
                vcProjectFileContent.AppendLine("  <ItemGroup>");

                foreach (var file in files)
                {
                    string fileType = GetFileType(file);
                    if (fileType == "None")
                        continue;

                    var projectPath = Utils.MakePathRelativeTo(file, projectDirectory);
                    vcProjectFileContent.AppendLine(string.Format("    <{0} Include=\"{1}\"/>", fileType, projectPath));
                }

                vcProjectFileContent.AppendLine("  </ItemGroup>");

                // IntelliSense information
                vcProjectFileContent.AppendLine("  <PropertyGroup>");
                vcProjectFileContent.AppendLine("    <NMakeForcedIncludes>$(NMakeForcedIncludes)</NMakeForcedIncludes>");
                vcProjectFileContent.AppendLine("    <NMakeAssemblySearchPath>$(NMakeAssemblySearchPath)</NMakeAssemblySearchPath>");
                vcProjectFileContent.AppendLine("    <NMakeForcedUsingAssemblies>$(NMakeForcedUsingAssemblies)</NMakeForcedUsingAssemblies>");
                vcProjectFileContent.AppendLine("  </PropertyGroup>");

                // End
                vcProjectFileContent.AppendLine("  <ItemDefinitionGroup>");
                vcProjectFileContent.AppendLine("  </ItemDefinitionGroup>");
                vcProjectFileContent.AppendLine("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />");
                vcProjectFileContent.AppendLine("  <ImportGroup Label=\"ExtensionTargets\">");
                vcProjectFileContent.AppendLine("  </ImportGroup>");
                vcProjectFileContent.AppendLine("</Project>");

            }

            // Filters file
            {
                vcFiltersFileContent.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
                vcFiltersFileContent.AppendLine(string.Format("<Project ToolsVersion=\"{0}\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">", projectFileToolVersion));

                // Files and folders
                var filtersDirectory = project.SourceFolderPath;
                vcFiltersFileContent.AppendLine("  <ItemGroup>");

                var filterDirectories = new HashSet<string>();
                foreach (var file in files)
                {
                    string fileType = GetFileType(file);
                    if (fileType == "None")
                        continue;

                    string projectPath = Utils.MakePathRelativeTo(file, projectDirectory);
                    string filterPath = Utils.MakePathRelativeTo(file, filtersDirectory);

                    int lastSeparatorIdx = filterPath.LastIndexOf(Path.DirectorySeparatorChar);
                    filterPath = lastSeparatorIdx == -1 ? string.Empty : filterPath.Substring(0, lastSeparatorIdx);
                    if (!string.IsNullOrWhiteSpace(filterPath))
                    {
                        // Add directories to the filters for this file
                        string pathRemaining = filterPath;
                        if (!filterDirectories.Contains(pathRemaining))
                        {
                            List<string> allDirectoriesInPath = new List<string>();
                            string currentPath = string.Empty;
                            while (true)
                            {
                                if (pathRemaining.Length <= 0)
                                    break;

                                string splitDirectory;
                                int slashIndex = pathRemaining.IndexOf(Path.DirectorySeparatorChar);
                                if (slashIndex != -1)
                                {
                                    splitDirectory = pathRemaining.Substring(0, slashIndex);
                                    pathRemaining = pathRemaining.Substring(splitDirectory.Length + 1);
                                }
                                else
                                {
                                    splitDirectory = pathRemaining;
                                    pathRemaining = string.Empty;
                                }

                                if (!string.IsNullOrEmpty(currentPath))
                                {
                                    currentPath += Path.DirectorySeparatorChar;
                                }

                                currentPath += splitDirectory;
                                allDirectoriesInPath.Add(currentPath);
                            }

                            // Filter directorires
                            foreach (var directory in allDirectoriesInPath)
                            {
                                if (!filterDirectories.Contains(directory))
                                {
                                    filterDirectories.Add(directory);

                                    string filterGuid = Guid.NewGuid().ToString("B").ToUpperInvariant();
                                    vcFiltersFileContent.AppendLine(string.Format("    <Filter Include=\"{0}\">", directory));
                                    vcFiltersFileContent.AppendLine(string.Format("      <UniqueIdentifier>{0}</UniqueIdentifier>", filterGuid));
                                    vcFiltersFileContent.AppendLine("    </Filter>");
                                }
                            }
                        }

                        vcFiltersFileContent.AppendLine(string.Format("    <{0} Include=\"{1}\">", fileType, projectPath));
                        vcFiltersFileContent.AppendLine(string.Format("      <Filter>{0}</Filter>", filterPath));
                        vcFiltersFileContent.AppendLine(string.Format("    </{0}>", fileType));
                    }
                    else
                    {

                        vcFiltersFileContent.AppendLine(string.Format("    <{0} Include=\"{1}\" />", fileType, projectPath));
                    }
                }
                vcFiltersFileContent.AppendLine("  </ItemGroup>");

                vcFiltersFileContent.AppendLine("</Project>");
            }

            // User file
            {
                vcUserFileContent.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
                vcUserFileContent.AppendLine(string.Format("<Project ToolsVersion=\"{0}\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">", projectFileToolVersion));
                vcUserFileContent.AppendLine("</Project>");
            }

            // Save the files
            Utils.WriteFileIfChanged(project.Path, vcProjectFileContent.ToString());
            Utils.WriteFileIfChanged(project.Path + ".filters", vcFiltersFileContent.ToString());
            Utils.WriteFileIfChanged(project.Path + ".user", vcUserFileContent.ToString());
        }

        public override void GenerateSolution(Solution solution)
        {
        }


        public static Guid GetProjectGuid(string path)
        {
            if (System.IO.File.Exists(path))
            {
                try
                {
                    XmlDocument doc = new XmlDocument();
                    doc.Load(path);

                    XmlNodeList elements = doc.GetElementsByTagName("ProjectGuid");
                    foreach (XmlElement element in elements)
                    {
                        return Guid.ParseExact(element.InnerText.Trim("{}".ToCharArray()), "D");
                    }
                }
                catch
                {
                    // Hide errors
                }
            }

            return Guid.NewGuid();
        }
    }
}
