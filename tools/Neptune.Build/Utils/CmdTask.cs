using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    [Serializable]
    public class CmdTask
    {
        public List<string> PrerequisiteFiles = new List<string>();
        public string WorkingDirectory = string.Empty;
        public string CommandPath = string.Empty;
        public string CommandArguments = string.Empty;
        public int Cost = 0;
        public string InfoMessage;
        public List<string> ProducedFiles = new List<string>();

        public HashSet<CmdTask> DependentTasks;
        public int DependentTasksCount => DependentTasks?.Count ?? 0;

        public int Result;
        public bool Failed => Result != 0;

        /// <summary>
        /// Gets a value indicating whether task results from the previous execution are still valid. Can be used to skip task execution.
        /// </summary>
        public bool HasValidCachedResults;

        public DateTime StartTime = DateTime.MinValue;
        public DateTime EndTime = DateTime.MinValue;
    }
}
