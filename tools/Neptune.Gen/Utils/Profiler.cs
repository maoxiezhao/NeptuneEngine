using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;

namespace Neptune.Gen
{
    public class ProfileEventScope : IDisposable
    {
        private readonly int _id;

        /// <summary>
        /// Initializes a new instance of the <see cref="ProfileEventScope"/> class.
        /// </summary>
        /// <param name="name">The event name.</param>
        public ProfileEventScope(string name)
        {
            _id = Profiler.Begin(name);
        }

        /// <summary>
        /// Ends the profiling event.
        /// </summary>
        public void Dispose()
        {
            Profiler.End(_id);
        }

        public static class Profiler
        {
            public struct Event
            {
                public string Name;
                public DateTime StartTime;
                public TimeSpan Duration;
                public int Depth;
                public int ThreadId;
            }

            private static int _depth;
            private static readonly List<Event> _events = new List<Event>(1024);
            private static readonly DateTime _startTime = DateTime.Now;
            private static readonly Stopwatch _stopwatch = Stopwatch.StartNew();

            public static int Begin(string name)
            {
                Event e;
                e.Name = name;
                e.StartTime = _startTime.AddTicks(_stopwatch.Elapsed.Ticks);
                e.Duration = TimeSpan.Zero;
                e.Depth = _depth++;
                e.ThreadId = Thread.CurrentThread.ManagedThreadId;
                _events.Add(e);
                return _events.Count - 1;
            }

            public static void End(int id)
            {
                var endTime = _startTime.AddTicks(_stopwatch.Elapsed.Ticks);
                var e = _events[id];
                e.Duration = endTime - e.StartTime;
                _events[id] = e;
                _depth--;
            }
        }
    }
}