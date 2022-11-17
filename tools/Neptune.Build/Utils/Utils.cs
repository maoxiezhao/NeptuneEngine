using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public static class Utils
    {
        /// <summary>
        /// Normalizes the path to the standard Flax format (all separators are '/' except for drive 'C:\').
        /// </summary>
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
    }
}
