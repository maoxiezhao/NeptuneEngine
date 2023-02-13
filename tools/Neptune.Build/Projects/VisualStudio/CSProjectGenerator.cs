using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class CSProjectGenerator : ProjectGenerator
    {
        public override string ProjectFileExtension => "csproj";
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

        public CSProjectGenerator(VisualStudioVersion version)
        {
            Version = version;
        }

        public override void GenerateProject(Project project)
        {
            var vsProject = project as VisualStudioProject;
            if (vsProject == null)
            {
                Log.Error("Unmatched project");
                return;
            }

            var content = new StringBuilder();
            var projectDirectory = Path.GetDirectoryName(project.Path);
            var projectFileToolVersion = ProjectFileToolVersion;
            var defaultTarget = project.Targets[0];
            var projectTypes = ProjectTypeGuids.ToOption(ProjectTypeGuids.WindowsCSharp);

            // Header
            content.AppendLine("<Project Sdk=\"Microsoft.NET.Sdk\">");

            // Properties
            content.AppendLine("  <PropertyGroup>");
            {
                switch (project.OutputType ?? defaultTarget.OutputType)
                {
                case TargetOutputType.Executable:
                    content.AppendLine("    <OutputType>Exe</OutputType>");
                    break;
                case TargetOutputType.Library:
                    content.AppendLine("    <OutputType>Library</OutputType>");
                    break;
                default: throw new ArgumentOutOfRangeException();
                }

                content.AppendLine(string.Format("    <RootNamespace>{0}</RootNamespace>", project.Name));
                content.AppendLine(string.Format("    <AssemblyName>{0}.CSharp</AssemblyName>", project.Name));
                content.AppendLine(string.Format("    <TargetFramework>{0}</TargetFramework>", "net6.0"));
                if (Version >= VisualStudioVersion.VS2022)
                    content.AppendLine("    <ResolveNuGetPackages>false</ResolveNuGetPackages>");
            }
            content.AppendLine("  </PropertyGroup>");

            // References
            content.AppendLine("  <ItemGroup>");
            {
                foreach (var reference in project.CSharp.SystemReferences)
                    content.AppendLine(string.Format("    <Reference Include=\"{0}\" />", reference));

                foreach (var reference in project.CSharp.FileReferences)
                {
                    content.AppendLine(string.Format("    <Reference Include=\"{0}\">", Path.GetFileNameWithoutExtension(reference)));
                    content.AppendLine(string.Format("      <HintPath>{0}</HintPath>", Utils.MakePathRelativeTo(reference, projectDirectory)));
                    content.AppendLine("    </Reference>");
                }
            }
            content.AppendLine("  </ItemGroup>");

            // Nones
            content.AppendLine("  <ItemGroup>");
            {
                var projectDir = Path.GetDirectoryName(project.Path);
                var files = Directory.GetFiles(projectDir, "*", SearchOption.TopDirectoryOnly);
                foreach (var file in files)
                {
                    string fileType;
                    if (!file.EndsWith(".cs", StringComparison.OrdinalIgnoreCase))
                    {
                        var projectPath = Utils.MakePathRelativeTo(file, projectDirectory);
                        content.AppendLine(string.Format("    <None Remove=\"{0}\" />", projectPath));
                    }
                }
            }
            content.AppendLine("  </ItemGroup>");

            // Files and folders
            content.AppendLine("  <ItemGroup>");
            {
                var files = new List<string>();
                if (project.SourceFiles != null)
                    files.AddRange(project.SourceFiles);
                if (project.SourceDirectories != null)
                {
                    foreach (var folder in project.SourceDirectories)
                        files.AddRange(Directory.GetFiles(folder, "*", SearchOption.AllDirectories));
                }

                foreach (var file in files)
                {
                    string fileType;
                    if (file.EndsWith(".cs", StringComparison.OrdinalIgnoreCase))
                        fileType = "Compile";
                    else
                        fileType = "None";

                    var projectPath = Utils.MakePathRelativeTo(file, projectDirectory);
                    content.AppendLine(string.Format("    <{0} Include=\"{1}\" />", fileType, projectPath));
                }
            }
            content.AppendLine("  </ItemGroup>");

            // End
            content.AppendLine("</Project>");

            // Save the files
            Utils.WriteFileIfChanged(project.Path, content.ToString());
        }

        public override void GenerateSolution(Solution solution)
        { 
            throw new NotImplementedException();
        }
    }
}
