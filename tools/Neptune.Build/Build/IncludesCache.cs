using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public static class IncludesCache
    {
        private static readonly Dictionary<string, string[]> AllIncludesCache = new Dictionary<string, string[]>();

        public static string[] FindAllIncludedFiles(string sourceFile)
        {
            string[] result;
            if (AllIncludesCache.TryGetValue(sourceFile, out result))
                return result;

            if (!System.IO.File.Exists(sourceFile))
                throw new Exception(string.Format("Cannot scan file \"{0}\" for includes because it does not exist.", sourceFile));

            var includedFiles = new HashSet<string>();
            includedFiles.Add(sourceFile);

            FindAllIncludedFiles(sourceFile, includedFiles);

            includedFiles.Remove(sourceFile);
            result = includedFiles.ToArray();
            AllIncludesCache.Add(sourceFile, result);

            return result;
        }

        private static void FindAllIncludedFiles(string sourceFile, HashSet<string> includedFiles)
        {
            var directIncludes = GetDirectIncludes(sourceFile);
            foreach (var includeFile in directIncludes)
            {
                if (includedFiles.Contains(includeFile))
                    continue;

                includedFiles.Add(includeFile);
                FindAllIncludedFiles(includeFile, includedFiles);
            }
        }

        private static readonly Dictionary<string, string[]> DirectIncludesCache = new Dictionary<string, string[]>();
        static readonly string IncludeToken = "include";
        private static string[] GetDirectIncludes(string sourceFile)
        {
            string[] result;
            if (DirectIncludesCache.TryGetValue(sourceFile, out result))
                return result;

            var includedFiles = new HashSet<string>();
            var sourceFileFolder = Path.GetDirectoryName(sourceFile);
            var fileContents = System.IO.File.ReadAllText(sourceFile, Encoding.UTF8);
            var length = fileContents.Length;
            for (int i = 0; i < length; i++)
            {
                // Skip single-line comments
                if (i < length - 1 && fileContents[i] == '/' && fileContents[i + 1] == '/')
                {
                    while (++i < length && fileContents[i] != '\n') ;
                    continue;
                }

                // Skip multi-line comments
                if (i > 0 && fileContents[i - 1] == '/' && fileContents[i] == '*')
                {
                    i++;
                    while (++i < length && !(fileContents[i - 1] == '*' && fileContents[i] == '/')) ;
                    i++;
                    continue;
                }

                // Read all until preprocessor instruction begin
                if (fileContents[i] != '#')
                    continue;

                // Skip spaces and tabs
                while (++i < length && (fileContents[i] == ' ' || fileContents[i] == '\t')) ;

                // Skip anything other than 'include' text
                if (i + IncludeToken.Length >= length)
                    break;

                var token = fileContents.Substring(i, IncludeToken.Length);
                if (token != IncludeToken)
                    continue;

                i += IncludeToken.Length;

                // Skip all before path start
                while (++i < length && fileContents[i] != '\n' && fileContents[i] != '"' && fileContents[i] != '<') ;

                // Skip all until path end
                var includeStart = i;
                while (++i < length && fileContents[i] != '\n' && fileContents[i] != '"' && fileContents[i] != '>') ;

                // Extract included file path
                var includedFile = fileContents.Substring(includeStart, i - includeStart);
                includedFile = includedFile.Trim();
                if (includedFile.Length == 0)
                    continue;

                includedFile = includedFile.Substring(1, includedFile.Length - 1);

                // Relative to the workspace root
                var includedFilePath = Path.Combine(Globals.Root, "Source", includedFile);
                if (!System.IO.File.Exists(includedFilePath))
                {
                    // Relative to the source file
                    includedFilePath = Path.Combine(sourceFileFolder, includedFile);
                    if (!System.IO.File.Exists(includedFilePath))
                    {
                        var isValid = false;

                        // Relative to any of the included project workspaces
                        var project = Globals.Project;
                        foreach (var reference in project.References)
                        {
                            includedFilePath = Path.Combine(reference.Project.ProjectFolderPath, "Source", includedFile);
                            if (System.IO.File.Exists(includedFilePath))
                            {
                                isValid = true;
                                break;
                            }
                        }

                        if (!isValid)
                        {
                            continue;
                        }
                    }
                }

                // Filter path
                includedFilePath = includedFilePath.Replace('\\', '/');
                includedFilePath = Utils.RemovePathRelativeParts(includedFilePath);

                includedFiles.Add(includedFilePath);
            }

            result = includedFiles.ToArray();
            DirectIncludesCache.Add(sourceFile, result);
            return result;
        }
    }
}
