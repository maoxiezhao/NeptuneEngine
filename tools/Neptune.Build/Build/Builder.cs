using System;
using System.IO;
using System.Reflection;
using System.Collections.Generic;
using System.Linq;

namespace Neptune.Build
{
    public static partial class Builder
    {
        public static void Clean()
        { 
        }

        public static bool BuildTargets()
        {
            var rules = GenerateRulesAssembly();
            var project = Globals.Project;
            if (project == null)
            {
                Log.Warning("Missing projects");
                return false;
            }

            bool ret = true;
            using (new ProfileEventScope("BuildTargets"))
            {
                var configBuildTargets = Configuration.BuildTargets;
                var projectTargets = GetProjectTargets(project);
                var targets = configBuildTargets == null ? projectTargets : projectTargets.Where(target => configBuildTargets.Contains(target.Name)).ToArray();
                if (targets.Length == 0)
                {
                    Log.Warning("No targets to build");
                    return false;
                }

                // Create task graph for building
                var taskGraph = new TaskGraph(project.ProjectFolderPath);
                foreach (var target in targets)
                {
                    target.PreBuild();

                    // Get target configurations
                    TargetConfiguration[] configurations = Configuration.BuildConfigurations;
                    if (configurations != null)
                    {
                        foreach (var configuration in configurations)
                        {
                            if (!target.Configurations.Contains(configuration))
                                throw new Exception(string.Format("Target {0} does not support {1} configuration.", target.Name, configuration));
                        }
                    }
                    else
                    {
                        configurations = target.Configurations;
                    }

                    // Pick platforms to build
                    TargetPlatform[] platforms = Configuration.BuildPlatforms;
                    if (platforms != null)
                    {
                        foreach (var platform in platforms)
                        {
                            if (!target.Platforms.Contains(platform))
                                throw new Exception(string.Format("Target {0} does not support {1} platform.", target.Name, platform));
                        }
                    }
                    else
                    {
                        platforms = target.Platforms;
                    }

                    // Pick architectures to build
                    TargetArchitecture[] architectures = Configuration.BuildArchitectures;
                    if (architectures != null)
                    {
                        foreach (var e in architectures)
                        {
                            if (!target.Architectures.Contains(e))
                                throw new Exception(string.Format("Target {0} does not support {1} architecture.", target.Name, e));
                        }
                    }
                    else
                    {
                        architectures = target.Architectures;
                    }

                    foreach (var configuration in configurations)
                    {
                        foreach (var targetPlatform in platforms)
                        {
                            foreach (var architecture in architectures)
                            {
                                if (!Platform.IsPlatformSupported(targetPlatform, architecture))
                                    continue;

                                var platform = Platform.GetPlatform(targetPlatform);
                                var toolchain = platform.GetToolchain(architecture);
                                var buildContext = new Dictionary<Target, BuildData>();

                                Log.Info(string.Format("Building target {0} in {1} for {2} {3}", target.Name, configuration, targetPlatform, architecture));
                                BuildTargetNativeCpp(buildContext, project, rules, taskGraph, target, toolchain, configuration);
                            }
                        }
                    }
                }

                // Prepare tasks
                using (new ProfileEventScope("PrepareTasks"))
                {
                    taskGraph.Setup();
                    taskGraph.SortTasks();
                    taskGraph.LoadCache();
                }

                // Execute tasks
                int executedTaskCount;
                if (!taskGraph.Execute(out executedTaskCount))
                {
                    return false;
                }

                // Save cache
                if (executedTaskCount > 0)
                {
                    taskGraph.SaveCache();
                }

                // Post build
                foreach (var target in targets)
                {
                    target.PostBuild();
                }
            }

            return ret;
        }
    }
}