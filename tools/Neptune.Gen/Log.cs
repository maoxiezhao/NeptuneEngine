using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class Log
    {
        private static object _consoleLocker = new object();

        public static string Indent = string.Empty;

        public static void Write(string message, ConsoleColor color, bool consoleLog = true)
        {
            if (consoleLog)
            {
                lock (_consoleLocker)
                {
                    Console.ForegroundColor = color;
                    Console.WriteLine(Indent + message);
                    Console.ResetColor();

                    if (System.Diagnostics.Debugger.IsAttached)
                        System.Diagnostics.Debug.WriteLine(Indent + message);
                }
            }
        }

        public static void Verbose(string message)
        {
            Write(message, Console.ForegroundColor, Configuration.Verbose);
        }

        public static void Info(string message)
        {
            Write(message, Console.ForegroundColor);
        }

        public static void Warning(string message)
        {
            Write(message, ConsoleColor.Yellow);
        }

        public static void Error(string message)
        {
            Write(message, ConsoleColor.Red);
        }

        public static void Exception(Exception ex)
        {
            Write(string.Format("Exception: {0}", ex.Message), ConsoleColor.Red);
            Write("Stack trace:", ConsoleColor.Yellow);
            Write(ex.StackTrace, ConsoleColor.Yellow);

            if (ex.InnerException != null)
            {
                Warning("Inner exception:");
                Exception(ex.InnerException);
            }
        }
    }
}
