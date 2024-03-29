﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using Neptune.Gen;

namespace Neptune.Gen
{
    public static class Utils
    {
        public static void AddRange<T>(this HashSet<T> source, IEnumerable<T> items)
        {
            foreach (T item in items)
            {
                source.Add(item);
            }
        }

        public static string NormalizePath(string path)
        {
            if (string.IsNullOrEmpty(path))
                return path;
            var chars = path.ToCharArray();

            // Convert all '\' to '/'
            for (int i = 0; i < chars.Length; i++)
            {
                if (chars[i] == '\\')
                    chars[i] = '/';
            }

            // Fix case 'C:/' to 'C:\'
            if (chars.Length > 2 && !char.IsDigit(chars[0]) && chars[1] == ':')
                chars[2] = '\\';

            return new string(chars);
        }

        public static string MakePathRelativeTo(string path, string directory)
        {
            int sharedDirectoryLength = -1;
            for (int i = 0; ; i++)
            {
                if (i == path.Length)
                {
                    // Paths are the same
                    if (i == directory.Length)
                    {
                        return string.Empty;
                    }

                    // Finished on a complete directory
                    if (directory[i] == Path.DirectorySeparatorChar)
                    {
                        sharedDirectoryLength = i;
                    }

                    break;
                }

                if (i == directory.Length)
                {
                    // End of the directory name starts with a boundary for the current name
                    if (path[i] == Path.DirectorySeparatorChar)
                    {
                        sharedDirectoryLength = i;
                    }

                    break;
                }

                if (string.Compare(path, i, directory, i, 1, StringComparison.OrdinalIgnoreCase) != 0)
                {
                    break;
                }

                if (path[i] == Path.DirectorySeparatorChar)
                {
                    sharedDirectoryLength = i;
                }
            }

            // No shared path found
            if (sharedDirectoryLength == -1)
            {
                return path;
            }

            // Add all the '..' separators to get back to the shared directory,
            StringBuilder result = new StringBuilder();
            for (int i = sharedDirectoryLength + 1; i < directory.Length; i++)
            {
                // Move up a directory
                result.Append("..");
                result.Append(Path.DirectorySeparatorChar);

                // Scan to the next directory separator
                while (i < directory.Length && directory[i] != Path.DirectorySeparatorChar)
                {
                    i++;
                }
            }

            if (sharedDirectoryLength + 1 < path.Length)
            {
                result.Append(path, sharedDirectoryLength + 1, path.Length - sharedDirectoryLength - 1);
            }

            return result.ToString();
        }

        public static string RemovePathRelativeParts(string path)
        {
            if (string.IsNullOrEmpty(path))
                return path;

            path = NormalizePath(path);

            string[] components = path.Split('/');
            Stack<string> stack = new Stack<string>();
            foreach (var bit in components)
            {
                if (bit == "..")
                {
                    if (stack.Count != 0)
                    {
                        var popped = stack.Pop();
                        if (popped == "..")
                        {
                            stack.Push(popped);
                            stack.Push(bit);
                        }
                    }
                    else
                    {
                        stack.Push(bit);
                    }
                }
                else if (bit == ".")
                {
                }
                else
                {
                    stack.Push(bit);
                }
            }

            bool isRooted = path.StartsWith("/");
            string result = string.Join(Path.DirectorySeparatorChar.ToString(), stack.Reverse());
            if (isRooted && result[0] != '/')
                result = result.Insert(0, "/");
            return result;
        }

        public static bool WriteFile(string path, string contents)
        {
            try
            {
                var directory = Path.GetDirectoryName(path);
                if (!string.IsNullOrEmpty(directory))
                    Directory.CreateDirectory(directory);

                using StreamWriter writer = new(path, false, new UTF8Encoding(false, true), 16 * 1024);
                writer.Write(contents);
                return true;
            }
            catch
            {
                Log.Error(string.Format("Failed to save file {0}", path));
                return false;
            }
        }

        public static bool RenameFile(string srcName, string dstName)
        {
            try
            {
                File.Move(srcName, dstName, true);
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        public static GenBuffer? ReadSourceToBuffer(string path)
        {
            if (!File.Exists(path))
            {
                return null;
            }

            try
            {
                using FileStream fs = new(path, FileMode.Open, FileAccess.Read, FileShare.Read, 4 * 1024, FileOptions.SequentialScan);
                using StreamReader sr = new(fs, Encoding.UTF8, true);
                GenBuffer initialBuffer = GenBufferPool.Alloc((int)fs.Length);
                int readLength = sr.Read(initialBuffer.Memory.Span);
                if (sr.EndOfStream)
                {
                    initialBuffer.Reset(readLength);
                    return initialBuffer;
                }
                else
                {
                    string remaining = sr.ReadToEnd();
                    long totalSize = readLength + remaining.Length;
                    GenBuffer combined = GenBufferPool.Alloc((int)totalSize);
                    Buffer.BlockCopy(initialBuffer.Block, 0, combined.Block, 0, readLength * sizeof(char));
                    Buffer.BlockCopy(remaining.ToArray(), 0, combined.Block, readLength * sizeof(char), remaining.Length * sizeof(char));
                    GenBufferPool.Free(initialBuffer);
                    return combined;
                }
            }
            catch (IOException)
            {
                return null;
            }
        }

        public static bool WriteSource(string path, ReadOnlySpan<char> contents)
        {
            try
            {
                var directory = Path.GetDirectoryName(path);
                if (!string.IsNullOrEmpty(directory))
                    Directory.CreateDirectory(directory);

                using StreamWriter writer = new(path, false, new UTF8Encoding(false, true), 16 * 1024);
                writer.Write(contents);
                return true;
            }
            catch
            {
                Log.Error(string.Format("Failed to save file {0}", path));
                return false;
            }
        }
    }
}
