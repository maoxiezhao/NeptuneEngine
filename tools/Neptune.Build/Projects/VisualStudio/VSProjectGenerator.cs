using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

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

        public override void GenerateProject(Project project)
        {
            var vsProject = project as VisualStudioProject;
            if (vsProject == null)
            {
                Log.Error("Unmatched project");
                return;
            }

            var projectDirectory = Path.GetDirectoryName(project.Path);

            var vcProjectFileContent = new StringBuilder();
            var vcFiltersFileContent = new StringBuilder();
            var vcUserFileContent = new StringBuilder();

            // Project file
            vcProjectFileContent.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
            vcProjectFileContent.AppendLine(string.Format("<Project DefaultTargets=\"Build\" ToolsVersion=\"{0}\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">", ProjectFileToolVersion));
            
            // Project configurations
            vcProjectFileContent.AppendLine("  <ItemGroup Label=\"ProjectConfigurations\">");
            foreach(var config in project.Configurations)
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
            vcProjectFileContent.AppendLine(string.Format("    <MinimumVisualStudioVersion>{0}</MinimumVisualStudioVersion>", ProjectFileToolVersion));
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
            foreach (var configuration in project.Configurations)
            {
                var targetBuildOptions = configuration.TargetBuildOptions;

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

                vcProjectFileContent.AppendLine("  </PropertyGroup>");
            }

            // Files and folders
            vcProjectFileContent.AppendLine("  <ItemGroup>");

            var files = new List<string>();
            if (project.SourceDirectories != null)
                files.AddRange(project.SourceFiles);

            if (project.SourceDirectories != null)
            {
                foreach (var folder in project.SourceDirectories)
                {
                    files.AddRange(Directory.GetFiles(folder, "*", SearchOption.AllDirectories));
                }
            }

            foreach (var file in files)
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
                    continue;
                }

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

            // Save the files
            Utils.WriteFileIfChanged(project.Path, vcProjectFileContent.ToString());
            Utils.WriteFileIfChanged(project.Path + ".filters", vcFiltersFileContent.ToString());
            Utils.WriteFileIfChanged(project.Path + ".user", vcUserFileContent.ToString());
        }

        public override void GenerateSolution(Solution solution)
        { 
        }
    }
}
