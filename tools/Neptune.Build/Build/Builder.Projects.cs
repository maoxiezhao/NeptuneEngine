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

            // Generate a set of configurations to build all targets for all platforms/configurations/architectures
            var rules = GenerateRulesAssembly();

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

                            // Get toolchain
                            var toolchain = platform.TryGetToolChain(architecture);
                            // Get build options
                            var buildOptions = GetBuildOptions(target, platform, toolchain, architecture, configuration, project.WorkspaceRootPath);
                            
                            // Collect modules
                            var modules = CollectModules(rules, platform, target, buildOptions, toolchain, architecture, configuration);
                            foreach (var module in modules)
                            {
                                module.Key.Setup(buildOptions);
                                module.Key.SetupEnvironment(buildOptions);
                            }

                            // Add a new configuration
                            string configurationText = targetName + '.' + platformName + '.' + configurationName;
                            var newConfig = new Project.ConfigurationData();
                            newConfig.Name = configurationText + '|' + architectureName;
                            newConfig.Text = configurationText;
                            newConfig.Platform = targetPlatform;
                            newConfig.PlatformName = platformName;
                            newConfig.Configuration = configuration;
                            newConfig.Target = target;
                            newConfig.Modules = modules;
                            newConfig.TargetBuildOptions = buildOptions;

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
            var projects = new List<Project>();
            Project mainSolutionProject = null;

            // Get projects from targets
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
                    var generator = ProjectGenerator.CreateCPP(projectFormat);
                    Project project = generator.CreateProject();
                    project.TargetType = TargetType.Cpp;
                    project.Name = projectName;
                    project.Targets = targets;
                    project.SearchPaths = new string[0];
                    project.WorkspaceRootPath = projectInfo.ProjectFolderPath;
                    project.Path = Path.Combine(projectsRoot, project.Name + '.' + generator.ProjectFileExtension);
                    project.SourceDirectories = new List<string> { Path.GetDirectoryName(targets[0].FilePath) };
                    if (project.WorkspaceRootPath.StartsWith(rootProject.ProjectFolderPath))
                        project.GroupName = Utils.MakePathRelativeTo(project.WorkspaceRootPath, rootProject.ProjectFolderPath);
                    else if (projectInfo != Globals.Project)
                        project.GroupName = projectInfo.Name;

                    // Setup configurations
                    SetupConfigurations(project, projectInfo);

                    var binaryModuleSet = new Dictionary<string, HashSet<Module>>();
                    var modulesBuildOptions = new Dictionary<Module, BuildOptions>();

                    foreach (var configurationData in project.Configurations)
                    {
                        var binaryModules = GetBinaryModules(projectInfo, configurationData.Target, configurationData.Modules);
                        foreach (var configurationBinaryModule in binaryModules)
                        {
                            // Skip if none of the included binary modules is inside the project workspace (eg. merged external binary modules from engine to game project)
                            if (!configurationBinaryModule.Any(y => y.FolderPath.StartsWith(project.WorkspaceRootPath)))
                                continue;

                            if (!binaryModuleSet.TryGetValue(configurationBinaryModule.Key, out var modulesSet))
                                binaryModuleSet[configurationBinaryModule.Key] = modulesSet = new HashSet<Module>();

                            // Collect module build options
                            foreach (var module in configurationBinaryModule)
                            {
                                modulesSet.Add(module);

                                if (!modulesBuildOptions.ContainsKey(module))
                                    modulesBuildOptions.Add(module, configurationData.Modules[module]);
                            }
                        }

                        foreach (var reference in projectInfo.References)
                        {
                            var referenceTargets = GetProjectTargets(reference.Project);
                            foreach (var referenceTarget in referenceTargets)
                            {
                                var refBuildOptions = GetBuildOptions(referenceTarget, configurationData.TargetBuildOptions.Platform, configurationData.TargetBuildOptions.Toolchain, configurationData.Architecture, configurationData.Configuration, reference.Project.ProjectFolderPath);
                                var refModules = CollectModules(rules, refBuildOptions.Platform, referenceTarget, refBuildOptions, refBuildOptions.Toolchain, refBuildOptions.Architecture, refBuildOptions.Configuration);
                                var refBinaryModules = GetBinaryModules(projectInfo, referenceTarget, refModules);
                                foreach (var binaryModule in refBinaryModules)
                                {
                                    project.Defines.Add(binaryModule.Key.ToUpperInvariant() + "_API=");
                                }
                            }
                        }
                    }

                    foreach (var binaryModule in binaryModuleSet)
                    {
                        project.Defines.Add(binaryModule.Key.ToUpperInvariant() + "_API=");
                    }

                    // Set main project
                    projects.Add(project);
                    if (mainSolutionProject == null && projectInfo == rootProject)
                        mainSolutionProject = project;
                }
            }

            // Generate projects
            using (new ProfileEventScope("GenerateProjects"))
            {
                foreach (var project in projects)
                {
                    Log.Info("Project " + project.Path);
                    project.Generate();
                }
            }

            // Generate C# project for build scripts files
            using (new ProfileEventScope("BuildScripts"))
            {
                // Create dummy target
                var target = new Target
                {
                    Name = "BuildScripts",
                    ProjectName = "BuildScripts",
                    FilePath = null,
                    FolderPath = null,
                    Type = TargetType.DotNet,
                    OutputType = TargetOutputType.Library
                };
                var dotNetGenerator = ProjectGenerator.CreateDotNet(projectFormat);
                Project project = dotNetGenerator.CreateProject();
                project.TargetType = TargetType.DotNet;
                project.Name = "BuildScripts";
                project.Targets = new[] { target };
                project.SearchPaths = new string[0];
                project.WorkspaceRootPath = workspaceRoot;
                project.Path = Path.Combine(projectsRoot, project.Name + '.' + dotNetGenerator.ProjectFileExtension);
                project.SourceFiles = new List<string>();
                foreach (var e in rules.Targets)
                    project.SourceFiles.Add(e.FilePath);
                foreach (var e in rules.Modules)
                    project.SourceFiles.Add(e.FilePath);

                // Setup configurations
                SetupConfigurations(project, rootProject);
                var c = project.Configurations[0];
                c.Name = "Debug|AnyCPU";
                c.Text = "Debug";
                project.Configurations[0] = c;

                // Add reference to Neptune.Build so rules can be validated in the IDE
                project.CSharp.FileReferences.Add(Assembly.GetEntryAssembly().Location);

                // Generate project
                dotNetGenerator.GenerateProject(project);

                projects.Add(project);
            }

            // Generate solution
            using (new ProfileEventScope("GenerateSolution"))
            {
                var generator = ProjectGenerator.CreateCPP(projectFormat);
                Solution solution = generator.CreateSolution();
                solution.Name = rootProject.Name;
                solution.WorkspaceRootPath = workspaceRoot;
                solution.Path = Path.Combine(workspaceRoot, solution.Name + '.' + generator.SolutionFileExtension);
                solution.Projects = projects.ToArray();
                solution.StartupProject = mainSolutionProject;

                Log.Info("Solution -> " + solution.Path);
                generator.GenerateSolution(solution);
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