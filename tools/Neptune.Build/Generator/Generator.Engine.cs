using Microsoft.VisualBasic;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using static Neptune.Build.Builder;

namespace Neptune.Build.Generator
{
    public static partial class CodeGenerator
    {
        public static void GenerateEngineVersionCPP(BuildData buildData)
        {
            var project = Globals.Project;
            if (project == null)
            {
                return;
            }

            var version = project.Version;
            var binaryModuleHeaderPath = Path.Combine(project.ProjectFolderPath, "Source", "EngineVersion.Gen.h");
            var binaryModuleNameUpper = "NEPTUNE";
            var contents = new StringBuilder();
            contents.AppendLine("// This code was auto-generated. Do not modify it.");
            contents.AppendLine();
            contents.AppendLine("#pragma once");
            contents.AppendLine();
            contents.AppendLine($"#define {binaryModuleNameUpper}_NAME \"{project.Name}\"");
            if (version.Build == -1)
                contents.AppendLine($"#define {binaryModuleNameUpper}_VERSION Version({version.Major}, {version.Minor})");
            else
                contents.AppendLine($"#define {binaryModuleNameUpper}_VERSION Version({version.Major}, {version.Minor}, {version.Build})");
            contents.AppendLine($"#define {binaryModuleNameUpper}_VERSION_TEXT \"{version}\"");
            contents.AppendLine($"#define {binaryModuleNameUpper}_VERSION_MAJOR {version.Major}");
            contents.AppendLine($"#define {binaryModuleNameUpper}_VERSION_MINOR {version.Minor}");
            contents.AppendLine($"#define {binaryModuleNameUpper}_VERSION_BUILD {version.Build}");
            contents.AppendLine($"#define {binaryModuleNameUpper}_COPYRIGHT \"{project.Copyright}\"");
            Console.WriteLine("Generate engine version file:" + binaryModuleHeaderPath);
            Utils.WriteFileIfChanged(binaryModuleHeaderPath, contents.ToString());
        }
    }
}
