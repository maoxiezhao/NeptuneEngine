using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public delegate CodeGenUnit CodeGenUnitDelegate(CodeGenFactory factory);

    public struct CodeGenExporter
    {
        public string Name;
        public string Description;
        public CodeGenUnitDelegate Delegate { get; }

        public CodeGenExporter(CodeGenAttribute attribute, CodeGenUnitDelegate genDelegate)
        {
            Name = attribute.Name;
            Description = attribute.Description;
            Delegate = genDelegate;
        }
    }

    public class CodeGenTables : IEnumerable<CodeGenExporter>
    {
        private Dictionary<string, CodeGenExporter> _codeGenExporters = new Dictionary<string, CodeGenExporter>();

        public void CollectCodeGenUnits()
        {
            var assembly = Assembly.GetExecutingAssembly();
            var types = assembly.GetTypes();
            for (var i = 0; i < types.Length; i++)
            {
                var type = types[i];
                if (!type.IsClass || type.IsAbstract)
                    continue;

                if (!type.IsSubclassOf(typeof(CodeGenUnit)))
                    continue;

                var methodInfos = type.GetMethods(BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic);
                foreach (System.Reflection.MethodInfo methodInfo in methodInfos)
                {
                    foreach (Attribute methodAttribute in methodInfo.GetCustomAttributes())
                    {
                        if (methodAttribute is CodeGenAttribute codeGenAttribute)
                        {
                            if (string.IsNullOrEmpty(codeGenAttribute.Name))
                                throw new Exception("CodeGenUnit must have a name.");

                            CodeGenExporter exporter = new(codeGenAttribute, (CodeGenUnitDelegate)Delegate.CreateDelegate(typeof(CodeGenUnitDelegate), methodInfo));
                            _codeGenExporters.Add(codeGenAttribute.Name, exporter);
                        }
                    }
                }
            }
        }

        public IEnumerator<CodeGenExporter> GetEnumerator()
        {
            foreach (KeyValuePair<string, CodeGenExporter> kvp in _codeGenExporters)
            {
                yield return kvp.Value;
            }
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            foreach (KeyValuePair<string, CodeGenExporter> kvp in _codeGenExporters)
            {
                yield return kvp.Value;
            }
        }
    }
}
