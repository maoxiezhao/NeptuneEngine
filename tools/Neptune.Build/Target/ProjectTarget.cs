using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class ProjectTarget : Target
    {
        public ProjectInfo Project;

        public override void Init()
        {
            base.Init();

            var projectFiles = Directory.GetFiles(Path.Combine(FolderPath, ".."), "*.nproj", SearchOption.TopDirectoryOnly);
            if (projectFiles.Length == 0)
                throw new Exception("Missing project file. Folder: " + FolderPath);
            else if (projectFiles.Length > 1)
                throw new Exception("Too many project files. Don't know which to pick. Folder: " + FolderPath);

            Project = ProjectInfo.Load(projectFiles[0]);
            ProjectName = OutputName = Project.Name;
        }

        public override void SetupTargetEnvironment(BuildOptions options)
        {
            base.SetupTargetEnvironment(options);

            foreach (var project in Project.GetAllProjects())
            {
                options.CompileEnv.IncludePaths.Add(Path.Combine(project.ProjectFolderPath, "Source"));
            }
        }
    }
}
