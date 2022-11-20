using System;
using System.Collections.Generic;
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

        public VSProjectGenerator(VisualStudioVersion version)
        { 
        }

        public override void GenerateSolution(Solution solution)
        { 
        }
    }
}
