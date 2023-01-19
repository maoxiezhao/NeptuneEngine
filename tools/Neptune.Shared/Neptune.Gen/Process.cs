using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using static Neptune.Gen.Manifest;

namespace Neptune.Gen
{
    public class Process
    {
        public Log.LogEventHandler LogInfoOutput = null;
        public bool GoWide { get; set; } = false;

        private Manifest _manifest;
        private ParsingSettings _parsingSettings;
        private FileParser _fileParser;
        private bool _mutex;
        private Dictionary<string, HeaderFile> _headerFileDictionary  = new Dictionary<string, HeaderFile>();
        private List<HeaderFile> _headerFiles = new List<HeaderFile>();
        private List<HeaderFile> _sortedHeaderFiles = new List<HeaderFile>();

        public IDGenerator HeaderFileIDGenerator { get; } = new IDGenerator();

        private enum TopologicalState
        {
            Unmarked,
            Temporary,
            Permanent,
        }

        public Process(Manifest manifest, ParsingSettings parsingSettings, bool mutex)
        {
            _manifest = manifest;
            _parsingSettings = parsingSettings;
            _mutex = mutex;
        }

        internal void PrepareHeaders()
        {
            using (new ProfileEventScope("PrepareHeaders"))
            {
                foreach (var module in _manifest.Modules)
                {
                    string moduleBasePath = Utils.NormalizePath(module.ModuleDirectory);
                    foreach (var headerFilePathOrigin in module.HeaderFiles)
                    {
                        string headerFilePath = Utils.NormalizePath(headerFilePathOrigin);
                        string headerFileName = Path.GetFileName(headerFilePath);
                        if (_headerFileDictionary.TryGetValue(headerFileName, out HeaderFile existingHeaderFile) && existingHeaderFile != null)
                        {
                            if (!String.Equals(existingHeaderFile.FilePath, headerFilePath, StringComparison.OrdinalIgnoreCase))
                            {
                                Log.Error($"Two headers with the same name is not allowed. '{existingHeaderFile.FilePath}' conflicts with '{existingHeaderFile.FilePath}'");
                                continue;
                            }
                        }

                        HeaderFile headerFile = new HeaderFile(this, headerFilePath);
                        _headerFiles.Add(headerFile);
                        _headerFileDictionary.Add(headerFileName, headerFile);

                        // Get the relative to the module base directory
                        if (headerFilePath.StartsWith(moduleBasePath, true, null))
                        {
                            int stripLength = moduleBasePath.Length;
                            if (stripLength < headerFilePath.Length && headerFilePath[stripLength] == '/')
                            {
                                ++stripLength;
                            }
                            headerFile.RelativeFilePath = headerFilePath.Substring(stripLength);
                        }
                    }
                }
            }
        }

        internal void ParseHeaders()
        {
            if (GoWide)
            {
                var taskGraph = new TaskGraph();
                foreach (var header in _headerFiles)
                {
                    var parsingTask = new ParsingTask(_fileParser, header);
                    taskGraph.Add(parsingTask);
                }

                using (new ProfileEventScope("ExecuteTasks"))
                {
                    taskGraph.SortTasks();

                    // Execute tasks
                    int executedTaskCount;
                    if (!taskGraph.Execute(out executedTaskCount))
                    {
                        throw new Exception("Failed to parse header files");
                    }
                }
            }
            else
            {
                foreach (var header in _headerFiles)
                {
                    var parsingTask = new ParsingTask(_fileParser, header);
                    parsingTask.Run();
                }
            }
        }

        private void SortHeaders()
        {
            using (new ProfileEventScope("SortHeaders"))
            {
                int count = HeaderFileIDGenerator.Count;
                _sortedHeaderFiles.Capacity = count;
                List<TopologicalState> states = new List<TopologicalState>(count);
                for (int index = 0; index < count; ++index)
                    states.Add(TopologicalState.Unmarked);

                foreach (HeaderFile headerFile in _headerFiles)
                {
                    if (states[headerFile.FileID] == TopologicalState.Unmarked)
                    {
                        HeaderFile recursion = TopologicalVisit(states, headerFile);
                        if (recursion != null)
                        {
                            throw new Exception(string.Format("Circular dependency detected:{0}", headerFile.FilePath));
                        }
                    }
                }
            }
        }

        private HeaderFile TopologicalVisit(List<TopologicalState> states, HeaderFile visit)
        {
            switch (states[visit.FileID])
            {
                case TopologicalState.Unmarked:
                    states[visit.FileID] = TopologicalState.Temporary;
                    foreach (HeaderFile referenced in visit.ReferencedHeadersNoLock)
                    {
                        if (referenced == visit)
                            continue;

                        HeaderFile recursion = TopologicalVisit(states, referenced);
                        if (recursion != null)
                        {
                            return recursion;
                        }
                    }

                    states[visit.FileID] = TopologicalState.Permanent;
                    _sortedHeaderFiles.Add(visit);
                    return null;

                case TopologicalState.Temporary:
                    return visit;

                case TopologicalState.Permanent:
                    return null;
                default:
                    throw new Exception("Unknown topological state");
            }
        }


        internal void StepGenerateFromHeaders()
        { 
            // Collect generators
        }

        public bool Run()
        {
            if (_parsingSettings == null)
            {
                Log.Error("Missing parsing settings.");
                return false;
            }

            Mutex singleInstanceMutex = null;
            try
            {
                if (_mutex)
                {
                    singleInstanceMutex = new Mutex(true, "Neptune.Gen", out var oneInstanceMutexCreated);
                    if (!oneInstanceMutexCreated)
                    {
                        try
                        {
                            if (!singleInstanceMutex.WaitOne(0))
                            {
                                Log.Warning("Wait for another instance(s) of Neptune.Build to end...");
                                singleInstanceMutex.WaitOne();
                            }
                        }
                        catch (AbandonedMutexException)
                        {
                            // Can occur if another Neptune.Build is killed in the debugger
                        }
                        finally
                        {
                            Log.Info("Waiting done.");
                        }
                    }
                }

                if (LogInfoOutput != null)
                    Log.OutputLogReceived += LogInfoOutput;

                using (new ProfileEventScope("GenerateProcess"))
                {
                    PrepareHeaders();
                    ParseHeaders();
                    SortHeaders();
                    StepGenerateFromHeaders();
                }
            }
            catch (Exception ex)
            {
                Log.Exception(ex);
                return false;
            }
            finally
            {
                if (LogInfoOutput != null)
                    Log.OutputLogReceived -= LogInfoOutput;

                if (singleInstanceMutex != null)
                {
                    singleInstanceMutex.Dispose();
                    singleInstanceMutex = null;
                }
            }

            return true;
        }
    }
}
