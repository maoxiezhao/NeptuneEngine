using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class CodeGenCache
    {
        public struct CacheFileInfo
        {
            public DateTime Dates;
            public bool IsValid;
        }

        public static Dictionary<string, CacheFileInfo> CacheFileInfos = new Dictionary<string, CacheFileInfo>();

        private static string GetCachePath()
        {
            // return Path.Combine(Globals.Output, "CodeGen.cache");
            return string.Empty;
        }

        public static readonly int CacheVersion = 1;
        public static bool LoadCache()
        {
            var path = GetCachePath();
            if (!File.Exists(path))
            {
                return false;
            }

            try
            {
                using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
                using (var reader = new BinaryReader(stream))
                {
                    int version = reader.ReadInt32();
                    if (version != CacheVersion)
                    {
                        Log.Error("Unsupported cache version ");
                        return false;
                    }

                    int filesCount = reader.ReadInt32();
                    for (int i = 0; i < filesCount; i++)
                    {
                        // Path
                        var file = reader.ReadString();
                        // Last File Write
                        var lastWrite = new DateTime(reader.ReadInt64());

                        var isValid = true;
                        if (File.Exists(file))
                        {
                            if (File.GetLastWriteTime(file) > lastWrite)
                            {
                                isValid = false;
                            }
                        }
                        else if (lastWrite != DateTime.MinValue)
                        {
                            isValid = false;
                        }

                        var cacheInfo = new CacheFileInfo
                        {
                            Dates = File.GetLastWriteTime(file),
                            IsValid = isValid
                        };
                        CacheFileInfos.Add(file, cacheInfo);
                    }
                    return true;
                }
            }
            catch
            {
                // Skip loading cache
                return false;
            }
        }

        public static void ValidCacheFileFromTasks(CodeGenUnit codeGenUnit, List<TaskBase> tasks)
        {
            var doneTasks = tasks.Where(task => (task.Result == 0 && task is CodeGenTask)).ToArray();
            foreach (var task in doneTasks)
            {
                CodeGenTask genTask = task as CodeGenTask;
                if (genTask == null || genTask.DependentTasks.Count <= 0)
                    continue;

                foreach (var depTask in genTask.DependentTasks)
                {
                    ParsingTask parsingTask = depTask as ParsingTask;
                    if (parsingTask == null)
                        continue;

                    string filePath = string.Empty; // .FilePath;
                    string generatedFilePath = codeGenUnit.GetGeneratedHeaderFilePath(filePath);
                    if (!File.Exists(generatedFilePath))
                        continue;

                    DateTime lastWriteTime = File.GetLastWriteTime(generatedFilePath);
                    if (CacheFileInfos.TryGetValue(filePath, out CacheFileInfo info))
                    {
                        info.IsValid = true;
                        info.Dates = lastWriteTime;
                    }
                    else
                    {
                        var cacheInfo = new CacheFileInfo
                        {
                            Dates = lastWriteTime,
                            IsValid = true
                        };
                        CacheFileInfos.Add(filePath, cacheInfo);
                    }
                }
            }

            List<string> toRemoveFiles = new List<string>();
            foreach (var kvp in CacheFileInfos)
            {
                CacheFileInfo cacheFileInfo = kvp.Value;
                if (!cacheFileInfo.IsValid) 
                {
                    toRemoveFiles.Add(kvp.Key);

                    string generatedHeaderPath = codeGenUnit.GetGeneratedHeaderFilePath(kvp.Key);
                    if (File.Exists(generatedHeaderPath))
                    {
                        File.Delete(generatedHeaderPath);
                    }

                    string generatedSourcePath = codeGenUnit.GetGeneratedSourceFilePath(kvp.Key);
                    if (File.Exists(generatedSourcePath))
                    {
                        File.Delete(generatedSourcePath);
                    }
                }
            }

            foreach(var toRemove in toRemoveFiles) 
            {
                CacheFileInfos.Remove(toRemove);
            }
        }

        public static void SaveCache()
        {
            var CachePath = GetCachePath();
            using (var stream = new FileStream(CachePath, FileMode.Create))
            using (var writer = new BinaryWriter(stream))
            {
                // Version
                writer.Write(CacheVersion);

                writer.Write(CacheFileInfos.Count);
                foreach (var kvp in CacheFileInfos)
                {
                    String file = kvp.Key;
                    CacheFileInfo info = kvp.Value;

                    // Path
                    writer.Write(file);

                    // Last File Write time
                    writer.Write(info.Dates.Ticks);
                }
            }
        }
    }
}
