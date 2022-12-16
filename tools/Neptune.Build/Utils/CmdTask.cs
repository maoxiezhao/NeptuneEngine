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
    }
}
