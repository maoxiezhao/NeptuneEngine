using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class Session
    {
        public Log.LogEventHandler LogInfoOutput = null;
        private Manifest _manifest;
        private ParsingSettings _parsingSettings;
        private bool _mutex;

        public Session(Manifest manifest, ParsingSettings parsingSettings, bool mutex)
        {
            _manifest = manifest;
            _parsingSettings = parsingSettings;
            _mutex = mutex;
        }

        public bool Run()
        {
            if (_parsingSettings == null || _manifest == null)
                return false;

            Mutex singleInstanceMutex = null;
            bool ret = true;
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

                ret = CodeGenManager.Generate(_parsingSettings);

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

            return ret;
        }
    }
}
