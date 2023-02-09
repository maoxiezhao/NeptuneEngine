using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using Newtonsoft.Json;

namespace Neptune.Build
{
    public class ProjectInfo
    {
        private static List<ProjectInfo> _projectsCache;

        [NonSerialized]
        public string ProjectPath;
        [NonSerialized]
        public string ProjectFolderPath;

        public string Name;
        public Version Version;
        public string Author = string.Empty;
        public string Copyright = string.Empty;

        public class Reference
        {
            public string Name;

            [NonSerialized]
            public ProjectInfo Project;

            /// <inheritdoc />
            public override string ToString()
            {
                return Name;
            }
        }
        public Reference[] References = new Reference[0];

        private void GetAllProjects(HashSet<ProjectInfo> result)
        {
            result.Add(this);
            foreach (var reference in References)
                reference.Project.GetAllProjects(result);
        }

        public HashSet<ProjectInfo> GetAllProjects()
        {
            var result = new HashSet<ProjectInfo>();
            GetAllProjects(result);
            return result;
        }

        public static ProjectInfo Load(string path)
        {
            path = Utils.RemovePathRelativeParts(path);
            if (_projectsCache == null)
                _projectsCache = new List<ProjectInfo>();

            for (int i = 0; i < _projectsCache.Count; i++)
            {
                if (_projectsCache[i].ProjectPath == path)
                    return _projectsCache[i];
            }

            try
            {
                Log.Info("Loading project file from \"" + path + "\"...");
                var contents = File.ReadAllText(path);
                ProjectInfo projectInfo = JsonConvert.DeserializeObject<ProjectInfo>(contents);
                projectInfo.ProjectPath = path;
                projectInfo.ProjectFolderPath = Path.GetDirectoryName(path);

                foreach (var reference in projectInfo.References)
                {
                    string referencePath;
                    if (reference.Name.StartsWith("$(EnginePath)"))
                    {
                        referencePath = Path.Combine(Globals.EngineRoot, reference.Name.Substring(14));
                    }
                    else if (reference.Name.StartsWith("$(ProjectPath)"))
                    {
                        referencePath = Path.Combine(projectInfo.ProjectFolderPath, reference.Name.Substring(15));
                    }
                    else if (Path.IsPathRooted(reference.Name))
                    {
                        referencePath = Path.Combine(Environment.CurrentDirectory, reference.Name);
                    }
                    else
                    {
                        referencePath = reference.Name;
                    }
                    referencePath = Utils.RemovePathRelativeParts(referencePath);

                    // Load referenced project
                    reference.Project = Load(referencePath);
                }

                // Project loaded
                Log.Info($"Loaded project {projectInfo.Name}, version {projectInfo.Version}");
                _projectsCache.Add(projectInfo);
                return projectInfo;
            }
            catch
            {
                // Failed to load project
                Log.Error("Failed to load project \"" + path + "\".");
                throw;
            }
        }

        public override int GetHashCode()
        {
            var hashCode = -833167044;
            hashCode = hashCode * -1521134295 + EqualityComparer<string>.Default.GetHashCode(Name);
            hashCode = hashCode * -1521134295 + EqualityComparer<string>.Default.GetHashCode(ProjectPath);
            hashCode = hashCode * -1521134295 + EqualityComparer<string>.Default.GetHashCode(ProjectFolderPath);
            hashCode = hashCode * -1521134295 + EqualityComparer<Version>.Default.GetHashCode(Version);
            hashCode = hashCode * -1521134295 + EqualityComparer<Reference[]>.Default.GetHashCode(References);
            return hashCode;
        }
    }
}
