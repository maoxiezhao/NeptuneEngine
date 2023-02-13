using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public abstract class ProjectGenerator
    {
        public abstract string ProjectFileExtension { get; }
        public abstract string SolutionFileExtension { get; }

        public static ProjectGenerator CreateCPP(ProjectFormat format)
        {
            switch (format)
            {
                case ProjectFormat.VisualStudio2022:
                {
                    return new VSProjectGenerator(VisualStudioVersion.VS2022);
                }
                default: throw new ArgumentOutOfRangeException(nameof(format), "Unknown project format.");
            }
        }

        public static ProjectGenerator CreateDotNet(ProjectFormat format)
        {
            switch (format)
            {
                case ProjectFormat.VisualStudio2022:
                {
                    return new CSProjectGenerator(VisualStudioVersion.VS2022);
                }
                default: throw new ArgumentOutOfRangeException(nameof(format), "Unknown project format.");
            }
        }

        public virtual Project CreateProject()
        {
            return new VisualStudioProject {
                Generator = this,
            };
        }
        public abstract void GenerateProject(Project project);

        public virtual Solution CreateSolution()
        {
            return new Solution();
        }
        public abstract void GenerateSolution(Solution solution);
    }
}
