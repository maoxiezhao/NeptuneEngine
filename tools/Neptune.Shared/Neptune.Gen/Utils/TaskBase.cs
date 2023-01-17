using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class TaskBase
    {
        public HashSet<TaskBase> DependentTasks = new HashSet<TaskBase>();
        public int DependentTasksCount => DependentTasks?.Count ?? 0;

        public int Result;
        public virtual bool Failed => Result != 0;

        public DateTime StartTime = DateTime.MinValue;
        public DateTime EndTime = DateTime.MinValue;

        public virtual int Run() { return 0; }
        public virtual string GetErrorMsg() { return ""; }
    }
}
