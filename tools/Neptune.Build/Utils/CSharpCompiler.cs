// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

using System;
using System.CodeDom.Compiler;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Globalization;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;

namespace Neptune.Build
{
    public sealed class ProviderOptions
    {
        /// <summary>
        /// The full path to csc.exe
        /// </summary>
        public string CompilerFullPath { get; internal set; }

        /// <summary>
        /// TTL in seconds
        /// </summary>
        public int CompilerServerTimeToLive { get; internal set; }

        /// <summary>
        /// Used by in-box framework code providers to determine which compat version of the compiler to use.
        /// </summary>
        public string CompilerVersion { get; internal set; }

        // smolloy todo debug degub - Does it really override everything? Is that the right thing to do?
        /// <summary>
        /// Treat all warnings as errors. Will override defaults and command-line options given for a compiler.
        /// </summary>
        public bool WarnAsError { get; internal set; }

        /// <summary>
        /// A collection of all &lt;providerOptions&gt; specified in config for the given CodeDomProvider.
        /// </summary>
        private IDictionary<string, string> _allOptions;
        public IDictionary<string, string> AllOptions
        {
            get
            {
                return _allOptions;
            }
            internal set
            {
                _allOptions = (value != null) ? new ReadOnlyDictionary<string, string>(value) : null;
            }
        }

        /// <summary>
        /// Create a default set of options for the C# and VB CodeProviders.
        /// </summary>
        public ProviderOptions()
        {
            this.CompilerFullPath = null;
            this.CompilerVersion = null;
            this.WarnAsError = false;

            // To be consistent, make sure there is always a dictionary here. It is less than ideal
            // for some parts of code to be checking AllOptions.count and some part checking
            // for AllOptions == null.
            this.AllOptions = new ReadOnlyDictionary<string, string>(new Dictionary<string, string>());

            // This results in no keep-alive for the compiler. This will likely result in
            // slower performance for any program that calls out the the compiler any
            // significant number of times. This is why the CompilerUtil.GetProviderOptionsFor
            // does not leave this as 0.
            this.CompilerServerTimeToLive = 0;
        }

        /// <summary>
        /// Create a set of options for the C# or VB CodeProviders using some specified inputs.
        /// </summary>
        public ProviderOptions(string compilerFullPath, int compilerServerTimeToLive) : this()
        {
            this.CompilerFullPath = compilerFullPath;
            this.CompilerServerTimeToLive = compilerServerTimeToLive;
        }
    }

    public class CSharpCompiler
    {
        private static volatile Regex outputRegWithFileAndLine;
        private static volatile Regex outputRegSimple;

        protected readonly ProviderOptions _providerOptions;
        private string _compilerFullPath = null;
        private const string CLR_PROFILING_SETTING = "COR_ENABLE_PROFILING";
        private const string DISABLE_PROFILING = "0";

        public CSharpCompiler(ProviderOptions providerOptions = null)
        {
            this._providerOptions = providerOptions;
        }

        protected string FileExtension
        {
            get
            {
                return ".cs";
            }
        }

        protected virtual string CompilerName
        {
            get
            {
                if (null == _compilerFullPath)
                {
                    _compilerFullPath = _providerOptions.CompilerFullPath;

                    // Try opening the file to make sure the compiler exist.  This will throw an exception
                    // if it doesn't
                    using (var str = File.OpenRead(_compilerFullPath)) { }
                }

                return _compilerFullPath;
            }
        }

        protected void ProcessCompilerOutputLine(CompilerResults results, string line)
        {
            if (outputRegSimple == null)
            {
                outputRegWithFileAndLine =
                    new Regex(@"(^(.*)(\(([0-9]+),([0-9]+)\)): )(error|warning) ([A-Z]+[0-9]+) ?: (.*)");
                outputRegSimple =
                    new Regex(@"(error|warning) ([A-Z]+[0-9]+) ?: (.*)");
            }

            //First look for full file info
            Match m = outputRegWithFileAndLine.Match(line);
            bool full;
            if (m.Success)
            {
                full = true;
            }
            else
            {
                m = outputRegSimple.Match(line);
                full = false;
            }

            if (m.Success)
            {
                var ce = new CompilerError();
                if (full)
                {
                    ce.FileName = m.Groups[2].Value;
                    ce.Line = int.Parse(m.Groups[4].Value, CultureInfo.InvariantCulture);
                    ce.Column = int.Parse(m.Groups[5].Value, CultureInfo.InvariantCulture);
                }

                if (string.Compare(m.Groups[full ? 6 : 1].Value, "warning", StringComparison.OrdinalIgnoreCase) == 0)
                {
                    ce.IsWarning = true;
                }

                ce.ErrorNumber = m.Groups[full ? 7 : 2].Value;
                ce.ErrorText = m.Groups[full ? 8 : 3].Value;
                results.Errors.Add(ce);
            }
        }

        protected string FullPathsOption
        {
            get
            {
                return " /fullpaths ";
            }
        }

        // CodeDom sets TreatWarningAsErrors to true whenever warningLevel is non-zero.
        // However, TreatWarningAsErrors should be false by default.
        // And users should be able to set the value by set the value of option "WarnAsError".
        // ASP.Net does fix this option in a like named function, but only for old CodeDom providers (CSharp/VB).
        // The old ASP.Net fix was to set TreatWarningAsErrors to false anytime '/warnaserror' was
        // detected in the compiler command line options, thus allowing the user-specified
        // option to prevail. In these CodeDom providers though, users have control through
        // the 'WarnAsError' provider option as well as manual control over the command
        // line args. 'WarnAsError' will default to false but can be set by the user.
        // So just go with the 'WarnAsError' provider option here.
        private void FixTreatWarningsAsErrors(CompilerParameters parameters)
        {
            parameters.TreatWarningsAsErrors = _providerOptions.WarnAsError;
        }

        protected void FixUpCompilerParameters(CompilerParameters options)
        {
            FixTreatWarningsAsErrors(options);
        }

        protected string CmdArgsFromParameters(CompilerParameters parameters)
        {
            var allArgsBuilder = new StringBuilder();

            if (parameters.GenerateExecutable)
            {
                allArgsBuilder.Append("/t:exe ");
                if (parameters.MainClass != null && parameters.MainClass.Length > 0)
                {
                    allArgsBuilder.Append("/main:");
                    allArgsBuilder.Append(parameters.MainClass);
                    allArgsBuilder.Append(" ");
                }
            }
            else
            {
                allArgsBuilder.Append("/t:library ");
            }

            // Get UTF8 output from the compiler
            allArgsBuilder.Append("/utf8output ");

            var coreAssembly = typeof(object).Assembly.Location;
            string coreAssemblyFileName = parameters.CoreAssemblyFileName;
            if (string.IsNullOrWhiteSpace(coreAssemblyFileName))
            {
                coreAssemblyFileName = coreAssembly;
            }

            if (!string.IsNullOrWhiteSpace(coreAssemblyFileName))
            {
                allArgsBuilder.Append("/nostdlib+ ");
                allArgsBuilder.Append("/R:\"").Append(coreAssemblyFileName.Trim()).Append("\" ");
            }

            // Bug 913691: Explicitly add System.Runtime as a reference.
            string systemRuntimeAssemblyPath = null;
            try
            {
                var systemRuntimeAssembly = Assembly.Load("System.Runtime, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b03f5f7f11d50a3a");
                systemRuntimeAssemblyPath = systemRuntimeAssembly.Location;
            }
            catch
            {
                // swallow any exceptions if we cannot find the assembly
            }
            if (systemRuntimeAssemblyPath != null)
            {
                allArgsBuilder.Append(string.Format("/R:\"{0}\" ", systemRuntimeAssemblyPath));
            }

            string systemCollectionsAssemblyPath = null;
            try
            {
                var systemRuntimeAssembly = Assembly.Load("System.Collections, Version = 6.0.0.0, Culture = neutral, PublicKeyToken = b03f5f7f11d50a3a");
                systemCollectionsAssemblyPath = systemRuntimeAssembly.Location;
            }
            catch
            {
                // swallow any exceptions if we cannot find the assembly
            }
            if (systemCollectionsAssemblyPath != null)
            {
                allArgsBuilder.Append(string.Format("/R:\"{0}\" ", systemCollectionsAssemblyPath));
            }

            foreach (string s in parameters.ReferencedAssemblies)
            {
                allArgsBuilder.Append("/R:");
                allArgsBuilder.Append("\"");
                allArgsBuilder.Append(s);
                allArgsBuilder.Append("\"");
                allArgsBuilder.Append(" ");
            }

            allArgsBuilder.Append("/out:");
            allArgsBuilder.Append("\"");
            allArgsBuilder.Append(parameters.OutputAssembly);
            allArgsBuilder.Append("\"");
            allArgsBuilder.Append(" ");

            if (parameters.IncludeDebugInformation)
            {
                allArgsBuilder.Append("/D:DEBUG ");
                allArgsBuilder.Append("/debug+ ");
                allArgsBuilder.Append("/optimize- ");
            }
            else
            {
                allArgsBuilder.Append("/debug- ");
                allArgsBuilder.Append("/optimize+ ");
            }

#if !FEATURE_PAL
            if (parameters.Win32Resource != null)
            {
                allArgsBuilder.Append("/win32res:\"" + parameters.Win32Resource + "\" ");
            }
#endif // !FEATURE_PAL

            foreach (string s in parameters.EmbeddedResources)
            {
                allArgsBuilder.Append("/res:\"");
                allArgsBuilder.Append(s);
                allArgsBuilder.Append("\" ");
            }

            foreach (string s in parameters.LinkedResources)
            {
                allArgsBuilder.Append("/linkres:\"");
                allArgsBuilder.Append(s);
                allArgsBuilder.Append("\" ");
            }

            if (parameters.TreatWarningsAsErrors)
            {
                allArgsBuilder.Append("/warnaserror+ ");
            }
            else
            {
                allArgsBuilder.Append("/warnaserror- ");
            }

            if (parameters.WarningLevel >= 0)
            {
                allArgsBuilder.Append("/w:" + parameters.WarningLevel + " ");
            }

            if (parameters.CompilerOptions != null)
            {
                allArgsBuilder.Append(parameters.CompilerOptions + " ");
            }

            string allArgs = allArgsBuilder.ToString();
            return allArgs;
        }

        private string GetCompilationArgumentString(CompilerParameters options)
        {
            FixUpCompilerParameters(options);

            return CmdArgsFromParameters(options);
        }

        private CompilerResults FromFileBatch(CompilerParameters options, string[] fileNames)
        {
            if (options == null)
            {
                throw new ArgumentNullException("options");
            }

            if (fileNames == null)
            {
                throw new ArgumentNullException("fileNames");
            }

            string outputFile = null;
            int retValue = 0;
            var results = new CompilerResults(options.TempFiles);

            bool createdEmptyAssembly = false;
            if (options.OutputAssembly == null || options.OutputAssembly.Length == 0)
            {
                string extension = (options.GenerateExecutable) ? "exe" : "dll";
                options.OutputAssembly = results.TempFiles.AddExtension(extension, !options.GenerateInMemory);

                // Create an empty assembly.  This is so that the file will have permissions that
                // we can later access with our current credential. If we don't do this, the compiler
                // could end up creating an assembly that we cannot open.
                new FileStream(options.OutputAssembly, FileMode.Create, FileAccess.ReadWrite).Close();
                createdEmptyAssembly = true;
            }

            var pdbname = "pdb";

            // Don't delete pdbs when debug=false but they have specified pdbonly.
            if (options.CompilerOptions != null
                    && -1 != CultureInfo.InvariantCulture.CompareInfo.IndexOf(options.CompilerOptions, "/debug:pdbonly", CompareOptions.IgnoreCase))
            {
                results.TempFiles.AddExtension(pdbname, true);
            }
            else
            {
                results.TempFiles.AddExtension(pdbname);
            }

            string args = GetCompilationArgumentString(options) + " " + JoinStringArray(fileNames, " ");

            // Use a response file if the compiler supports it
            string responseFileArgs = GetResponseFileCmdArgs(options, args);
            string trueArgs = null;
            if (responseFileArgs != null)
            {
                trueArgs = args;
                args = responseFileArgs;
            }

            // Appending TTL to the command line arguments.
            if (_providerOptions.CompilerServerTimeToLive > 0)
            {
                args = string.Format("/shared /keepalive:\"{0}\" {1}", _providerOptions.CompilerServerTimeToLive, args);
            }

            Compile(options,
                CompilerName,
                args,
                ref outputFile,
                ref retValue);

            results.NativeCompilerReturnValue = retValue;

            // only look for errors/warnings if the compile failed or the caller set the warning level
            if (retValue != 0 || options.WarningLevel > 0)
            {

                // The output of the compiler is in UTF8
                string[] lines = ReadAllLines(outputFile, Encoding.UTF8, FileShare.ReadWrite);
                bool replacedArgs = false;
                foreach (string line in lines)
                {
                    if (!replacedArgs && trueArgs != null && line.Contains(args))
                    {
                        replacedArgs = true;
                        var outputLine = string.Format("{0}>{1} {2}",
                            Environment.CurrentDirectory,
                            CompilerName,
                            trueArgs);
                        results.Output.Add(outputLine);
                    }
                    else
                    {
                        results.Output.Add(line);
                    }

                    ProcessCompilerOutputLine(results, line);
                }

                // Delete the empty assembly if we created one
                if (retValue != 0 && createdEmptyAssembly)
                {
                    File.Delete(options.OutputAssembly);
                }
            }

            if (retValue != 0 || results.Errors.HasErrors || !options.GenerateInMemory)
            {

                results.PathToAssembly = options.OutputAssembly;
                return results;
            }

            // Read assembly into memory:
            byte[] assemblyBuff = File.ReadAllBytes(options.OutputAssembly);

            // Read symbol file into memory and ignore any errors that may be encountered:
            // (This functionality was added in NetFx 4.5, errors must be ignored to ensure compatibility)
            byte[] symbolsBuff = null;
            try
            {

                string symbFileName = options.TempFiles.BasePath + "." + pdbname;

                if (File.Exists(symbFileName))
                {
                    symbolsBuff = File.ReadAllBytes(symbFileName);
                }
            }
            catch
            {
                symbolsBuff = null;
            }

            results.CompiledAssembly = Assembly.Load(assemblyBuff, symbolsBuff);
            return results;
        }

        public CompilerResults CompileAssemblyFromFile(CompilerParameters options, string fileName)
        {
            if (options == null)
            {
                throw new ArgumentNullException("options");
            }

            if (fileName == null)
            {
                throw new ArgumentNullException("fileName");
            }

            return CompileAssemblyFromFileBatch(options, new string[] { fileName });
        }

        public CompilerResults CompileAssemblyFromFileBatch(CompilerParameters options, string[] fileNames)
        {
            if (options == null)
            {
                throw new ArgumentNullException("options");
            }

            if (fileNames == null)
            {
                throw new ArgumentNullException("fileNames");
            }

            try
            {
                // Try opening the files to make sure they exists.  This will throw an exception
                // if it doesn't
                foreach (var fileName in fileNames)
                {
                    using (var str = File.OpenRead(fileName)) { }
                }

                return FromFileBatch(options, fileNames);
            }
            finally
            {
                options.TempFiles.Delete();
            }
        }

        private void Compile(CompilerParameters options, string compilerFullPath, string arguments,
                              ref string outputFile, ref int nativeReturnValue)
        {
            string errorFile = null;
            string cmdLine = "\"" + compilerFullPath + "\" " + arguments;
            outputFile = options.TempFiles.AddExtension("out");

            bool profilingSettingIsUpdated = false;
            string originalClrProfilingSetting = null;

            // if CLR_PROFILING_SETTING is not set in environment variables, this returns null
            originalClrProfilingSetting = Environment.GetEnvironmentVariable(CLR_PROFILING_SETTING, EnvironmentVariableTarget.Process);
            // if CLR profiling is already disabled, don't bother to set it again
            if (originalClrProfilingSetting != DISABLE_PROFILING)
            {
                Environment.SetEnvironmentVariable(CLR_PROFILING_SETTING, DISABLE_PROFILING, EnvironmentVariableTarget.Process);
                profilingSettingIsUpdated = true;
            }

            nativeReturnValue = Executor.ExecWaitWithCapture(
                options.UserToken,
                cmdLine,
                Environment.CurrentDirectory,
                options.TempFiles,
                ref outputFile,
                ref errorFile);

            if (profilingSettingIsUpdated)
            {
                Environment.SetEnvironmentVariable(CLR_PROFILING_SETTING, originalClrProfilingSetting, EnvironmentVariableTarget.Process);
            }
        }

        private string GetResponseFileCmdArgs(CompilerParameters options, string cmdArgs)
        {

            string responseFileName = options.TempFiles.AddExtension("cmdline");
            var responseFileStream = new FileStream(responseFileName, FileMode.Create, FileAccess.Write, FileShare.Read);
            try
            {
                using (var sw = new StreamWriter(responseFileStream, Encoding.UTF8))
                {
                    sw.Write(cmdArgs);
                    sw.Flush();
                }
            }
            finally
            {
                responseFileStream.Close();
            }

            // Always specify the /noconfig flag (outside of the response file)
            return "/noconfig " + FullPathsOption + "@\"" + responseFileName + "\"";
        }

        private static string JoinStringArray(string[] sa, string separator)
        {
            if (sa == null || sa.Length == 0)
            {
                return String.Empty;
            }

            if (sa.Length == 1)
            {
                return "\"" + sa[0] + "\"";
            }

            var sb = new StringBuilder();
            for (int i = 0; i < sa.Length - 1; i++)
            {
                sb.Append("\"");
                sb.Append(sa[i]);
                sb.Append("\"");
                sb.Append(separator);
            }

            sb.Append("\"");
            sb.Append(sa[sa.Length - 1]);
            sb.Append("\"");

            return sb.ToString();
        }

        private static string[] ReadAllLines(String file, Encoding encoding, FileShare share)
        {
            using (var stream = File.Open(file, FileMode.Open, FileAccess.Read, share))
            {
                String line;
                var lines = new List<String>();

                using (var sr = new StreamReader(stream, encoding))
                {
                    while ((line = sr.ReadLine()) != null)
                    {
                        lines.Add(line);
                    }
                }

                return lines.ToArray();
            }
        }
    }
}
