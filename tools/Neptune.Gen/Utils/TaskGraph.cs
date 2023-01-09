using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class TaskGraph
    {
        public void Setup()
        {
        }

        public bool Execute(out int executedTasksCount)
        {
            executedTasksCount = 0;
            return true;
        }

        public void LoadCache()
        {

        }

        public void SaveCache()
        {
        }
    }
}
