using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class IDGenerator
    {
        private int _count = 0;

        public int GenerateID()
        {
            return Interlocked.Increment(ref _count) - 1;
        }

        public int Count => Interlocked.Add(ref _count, 0) + 1;
    }
}
