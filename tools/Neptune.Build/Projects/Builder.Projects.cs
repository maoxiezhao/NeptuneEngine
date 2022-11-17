using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using static System.Net.Mime.MediaTypeNames;
using System.Xml.Linq;

namespace Neptune.Build
{
    partial class Builder
    {
        private static void SetupConfigurations(Project project, ProjectInfo projectInfo)
        {
            project.Configurations.Clear();

            foreach (var target in project.Targets)
            {
                string targetName = target.Name;

                foreach (var targetPlatform in target.Platforms)
                {
                    string platformName = targetPlatform.ToString();
                    foreach (var configuration in target.Configurations)
                    {
                        string configurationName = configuration.ToString();
                        foreach (var architecture in target.Architectures)
                        {
                            string architectureName = architecture.ToString();

                            // Check current platform is support
                            if (!Platform.IsPlatformSupported(targetPlatform, architecture))
                                continue;

                            var platform = Platform.GetPlatform(targetPlatform);
                            if (platform == null)
                                continue;

                            if (!platform.HasRequiredSDKsInstalled)
                                continue;

                            // Add a new configuration
                            string configurationText = targetName + '.' + platformName + '.' + configurationName;
                            var newConfig = new Project.ConfigurationData();
                            newConfig.Name = configurationText + '|' + architectureName;
                            newConfig.Text = configurationText;
                            newConfig.Platform = targetPlatform;
                            newConfig.PlatformName = platformName;
                            newConfig.Configuration = configuration;
                            newConfig.Target = target;
                            project.Configurations.Add(newConfig);
                        }
                    }
                }
            }
        }

        public static void GenerateProject(ProjectFormat projectFormat)
        {
            var rootProject = Globals.Project;
            if (rootProject == null)
                throw new Exception("Missing project.");

            var projectFiles = rootProject.GetAllProjects();
            var rules = GenerateRulesAssembly();

            var workspaceRoot = rootProject.ProjectFolderPath;
            var projectsRoot = Path.Combine(workspaceRoot, "Cache", "Projects");

            var targetGroups = rules.Targets.GroupBy(x => x.ProjectName);
            foreach (var e in targetGroups)
            {
                var projectName = e.Key;
                using (new ProfileEventScope(projectName))
                {
                    var targets = e.ToArray();
                    if (targets.Length == 0)
                        throw new Exception("No targets in a group " + projectName);

                    var projectInfo = projectFiles.First(x => targets[0].FolderPath.Contains(x.ProjectFolderPath));

                    // Create project
                    var generator = ProjectGenerator.Create(projectFormat);
                    Project project = generator.CreateProject();
                    project.Name = projectName;
                    project.Targets = targets;
                    project.WorkspaceRootPath = projectInfo.ProjectFolderPath;
                    project.Path = Path.Combine(projectsRoot, project.Name + '.' + generator.ProjectFileExtension);
                    project.SourceDirectories = new List<string> { Path.GetDirectoryName(targets[0].FilePath) };
                    
                    // Setup configurations
                    SetupConfigurations(project, projectInfo);

                    foreach (var configurationData in project.Configurations)
                    { 
                    }
                }
            }
        }

        public static void GenerateProjects()
        {
            using (new ProfileEventScope("GenerateProjects"))
            {
                ProjectFormat format = ProjectFormat.Unknown;
                if (Configuration.ProjectVS2022)
                    format = ProjectFormat.VisualStudio2022;

                if (format == ProjectFormat.Unknown)
                {
                    throw new Exception(string.Format("Unknown project format for project generating."));
                }

                GenerateProject(format);
            }
        }
    }
}