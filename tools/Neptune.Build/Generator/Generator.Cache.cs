using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Remoting.Messaging;
using System.Text;
using System.Threading.Tasks;
using static Neptune.Build.Generator.CodeGenerator;

namespace Neptune.Build.Generator
{
    public static partial class CodeGenerator
    {
        internal class FileCacheInfo
        {
            public long LastWriteTime;
            public bool bContainsMarkup;
        }

        internal static Dictionary<string, FileCacheInfo> fileToCacheInfo = new Dictionary<string, FileCacheInfo>();
        internal static bool isDirty = false;

        public static bool IsCurrentModuleDirty()
        {
            return isDirty;
        }

        internal static string Read(BinaryReader reader, string e)
        {
            var valid = reader.ReadBoolean();
            if (valid)
                e = reader.ReadString();
            return e;
        }

        internal static string[] Read(BinaryReader reader, string[] list)
        {
            var count = reader.ReadInt32();
            if (count != 0)
            {
                list = new string[count];
                for (int i = 0; i < count; i++)
                    list[i] = reader.ReadString();
            }
            return list;
        }

        internal static void Write(BinaryWriter writer, HashSet<string> list)
        {
            if (list != null)
            {
                writer.Write(list.Count());
                foreach (var e in list)
                    writer.Write(e);
            }
            else
            {
                writer.Write(0);
            }
        }

        internal static void Write(BinaryWriter writer, List<string> list)
        {
            if (list != null)
            {
                writer.Write(list.Count());
                foreach (var e in list)
                    writer.Write(e);
            }
            else
            {
                writer.Write(0);
            }
        }

        internal static bool ContainsReflectionMarkupInner(string headerFile)
        {
            string headerFileContents = File.ReadAllText(headerFile);
            var apiTokens = GetParsingSettings().ApiTokens.GetSearchTags();
            for (int j = 0; j < apiTokens.Count; j++)
            {
                if (headerFileContents.Contains(apiTokens[j]))
                {
                    return true;
                }
            }

            return false;
        }

        private static bool ContainsReflectionMarkup(string headerFile)
        {
            FileCacheInfo fileCacheInfo = null;
            if (!fileToCacheInfo.TryGetValue(headerFile, out fileCacheInfo) || 
                File.GetLastWriteTime(headerFile).Ticks > fileCacheInfo.LastWriteTime)
            {
                fileToCacheInfo[headerFile] = new FileCacheInfo
                {
                    LastWriteTime = File.GetLastWriteTime(headerFile).Ticks,
                    bContainsMarkup = ContainsReflectionMarkupInner(headerFile)
                };
                isDirty = true;
            }
            return fileCacheInfo.bContainsMarkup;
        }

        private const int CacheVersion = 1;
        private static string GetCachePath(Module module, BuildOptions moduleOptions)
        {
            return Path.Combine(moduleOptions.IntermediateFolder, module.Name + ".Gen.cache");
        }

        private static bool LoadCache(GeneratorModuleInfo moduleInfo, BuildOptions moduleOptions, List<string> headerFiles)
        {
            isDirty = true;
            fileToCacheInfo.Clear();

            var path = GetCachePath(moduleInfo.ModuleInst, moduleOptions);
            if (!File.Exists(path))
                return false;

            try
            {
                using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
                using (var reader = new BinaryReader(stream, Encoding.UTF8))
                {
                    // Version
                    var version = reader.ReadInt32();
                    if (version != CacheVersion)
                        return false;
                    if (File.GetLastWriteTime(Assembly.GetExecutingAssembly().Location).Ticks != reader.ReadInt64())
                        return false;

                    // Build options
                    if (reader.ReadString() != moduleOptions.IntermediateFolder ||
                        reader.ReadInt32() != (int)moduleOptions.Platform.Target ||
                        reader.ReadInt32() != (int)moduleOptions.Architecture ||
                        reader.ReadInt32() != (int)moduleOptions.Configuration)
                        return false;
                    var publicDefinitions = Read(reader, Utils.GetEmptyArray<string>());
                    if (publicDefinitions.Length != moduleOptions.PublicDefinitions.Count || publicDefinitions.Any(x => !moduleOptions.PublicDefinitions.Contains(x)))
                        return false;
                    var privateDefinitions = Read(reader, Utils.GetEmptyArray<string>());
                    if (privateDefinitions.Length != moduleOptions.PrivateDefinitions.Count || privateDefinitions.Any(x => !moduleOptions.PrivateDefinitions.Contains(x)))
                        return false;
                    var preprocessorDefinitions = Read(reader, Utils.GetEmptyArray<string>());
                    if (preprocessorDefinitions.Length != moduleOptions.CompileEnv.PreprocessorDefinitions.Count || preprocessorDefinitions.Any(x => !moduleOptions.CompileEnv.PreprocessorDefinitions.Contains(x)))
                        return false;

                    // Read cached header files
                    var headerFilesCount = reader.ReadInt32();
                    for (int i = 0; i < headerFilesCount; i++)
                    {
                        var headerFile = reader.ReadString();
                        fileToCacheInfo[headerFile] = new FileCacheInfo {
                            LastWriteTime = reader.ReadInt64(),
                            bContainsMarkup = reader.ReadBoolean()
                        };
                    }

                    if (headerFiles.Count != headerFilesCount)
                        return false;

                    isDirty = false;
                    return true;
                }
            }
            catch
            {
                // Skip loading cache
                return false;
            }
        }

        private static void SaveCache(GeneratorModuleInfo moduleInfo, BuildOptions moduleOptions, List<string> headerFiles)
        {
            if (!isDirty || !Directory.Exists(moduleOptions.IntermediateFolder))
                return;

            var path = GetCachePath(moduleInfo.ModuleInst, moduleOptions);
            using (var stream = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.Read))
            using (var writer = new BinaryWriter(stream, Encoding.UTF8))
            {
                // Version
                writer.Write(CacheVersion);
                writer.Write(File.GetLastWriteTime(Assembly.GetExecutingAssembly().Location).Ticks);

                // Build options
                writer.Write(moduleOptions.IntermediateFolder);
                writer.Write((int)moduleOptions.Platform.Target);
                writer.Write((int)moduleOptions.Architecture);
                writer.Write((int)moduleOptions.Configuration);
                Write(writer, moduleOptions.PublicDefinitions);
                Write(writer, moduleOptions.PrivateDefinitions);
                Write(writer, moduleOptions.CompileEnv.PreprocessorDefinitions);

                // Header files
                writer.Write(headerFiles.Count);
                for (int i = 0; i < headerFiles.Count; i++)
                {
                    var headerFile = headerFiles[i];
                    writer.Write(headerFile);
                    if (fileToCacheInfo.TryGetValue(headerFile, out var fileCacheInfo))
                    {
                        writer.Write(fileCacheInfo.LastWriteTime);
                        writer.Write(fileCacheInfo.bContainsMarkup);
                    }
                    else
                    {
                        writer.Write(File.GetLastWriteTime(headerFile).Ticks);
                        writer.Write(false);
                    }
                }
            }
        }
    }
}
