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

        public VSProjectGenerator(VisualStudioVersion version)
        { 
        }
    }
}
