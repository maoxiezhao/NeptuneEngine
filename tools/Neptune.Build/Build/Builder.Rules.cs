using System;
using System.IO;
using System.Reflection;
using System.Collections.Generic;
using System.Linq;

namespace Neptune.Build
{
    public static partial class Builder
    {
        public static string BuildFilesPostfix = ".Build.cs";

        private static RulesAssembly _rules;
        private static void FindRules(string directory, List<string> result)
        {
            var files = Directory.GetFiles(directory);
            for (int i = 0; i < files.Length; i++)
            {
                var file = files[i];
                if (file.EndsWith(BuildFilesPostfix, StringComparison.OrdinalIgnoreCase))
                {
                    result.Add(file);
                }
            }

            var directories = Directory.GetDirectories(directory);
            for (int i = 0; i < directories.Length; i++)
            {
                FindRules(directories[i], result);
            }
        }

        public static RulesAssembly GenerateRulesAssembly()
        {
            if (_rules != null)
                return _rules;

            // Use rules from any of the included projects workspace
            using (new ProfileEventScope("GenerateRulesAssembly"))
            {
                var files = new List<string>();
                if (Globals.Project != null)
                {
                    var projects = Globals.Project.GetAllProjects();
                    foreach (var project in projects)
                    {
                        var sourceFolder = Path.Combine(project.ProjectFolderPath, "Source");
                        if (Directory.Exists(sourceFolder))
                            FindRules(sourceFolder, files);
                    }
                }

                // Compile code
                Assembly assembly;
                using (new ProfileEventScope("CompileRules"))
                {
                    var assembler = new Assembler();
                    assembler.SourceFiles.AddRange(files);
                    assembly = assembler.Build();
                }

                // Prepare targets and modules objects
                Type[] types;
                var targetObjects = new List<Target>(16);
                var moduleObjects = new List<Module>(256);
                types = assembly.GetTypes();
                for (var i = 0; i < types.Length; i++)
                {
                    var type = types[i];
                    if (!type.IsClass || type.IsAbstract)
                        continue;

                    if (type.IsSubclassOf(typeof(Target)))
                    {
                        var target = (Target)Activator.CreateInstance(type);
                        var targetFilename = target.Name + BuildFilesPostfix;
                        target.FilePath = files.FirstOrDefault(path => string.Equals(Path.GetFileName(path), targetFilename, StringComparison.OrdinalIgnoreCase));
                        if (target.FilePath == null)
                        {
                            throw new Exception(string.Format("Failed to find source file path for {0}", target));
                        }

                        target.FolderPath = Path.GetDirectoryName(target.FilePath);
                        target.Init();
                        targetObjects.Add(target);
                    }
                    else if (type.IsSubclassOf(typeof(Module)))
                    {
                        var module = (Module)Activator.CreateInstance(type);

                        var moduleFilename = module.Name + BuildFilesPostfix;
                        module.FilePath = files.FirstOrDefault(path => string.Equals(Path.GetFileName(path), moduleFilename, StringComparison.OrdinalIgnoreCase));
                        if (module.FilePath == null)
                        {
                            throw new Exception(string.Format("Failed to find source file path for {0}", module));
                        }

                        module.FolderPath = Path.GetDirectoryName(module.FilePath);
                        module.Init();
                        moduleObjects.Add(module);
                    }
                }

                _rules = new RulesAssembly(assembly, targetObjects.ToArray(), moduleObjects.ToArray());
            }

            return _rules;
        }

        public static Target[] GetProjectTargets(ProjectInfo project)
        {
            var rules = GenerateRulesAssembly();
            var sourcePath = Path.Combine(project.ProjectFolderPath, "Source");
            return rules.Targets.Where(target => target.FolderPath.StartsWith(sourcePath)).ToArray();
        }

        public static void Clean()
        { 
        }

        public static bool BuildTargets()
        {
            var project = Globals.Project;
            if (project == null)
            {
                Log.Warning("Missing projects");
                return false;
            }

            bool ret = true;
            var taskGraph = new TaskGraph("");

            // Prepare tasks
            taskGraph.Setup();

            taskGraph.LoadCache();

            // Execute tasks
            int executedTaskCount;
            if (!taskGraph.Execute(out executedTaskCount))
                return false;

            // Save cache
            if (executedTaskCount > 0)
            {
                taskGraph.SaveCache();
            }

            // Post build

            return ret;
        }
    }
}