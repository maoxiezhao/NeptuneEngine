using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class StructClassTree
    {
        private struct InheritanceLink
        {
            public string ClassName;
            EAccessSpecifier AccessSpecifier;
            public InheritanceLink(string name, EAccessSpecifier accessSpecifier)
            {
                ClassName = name;
                AccessSpecifier = accessSpecifier;
            }
        }
        private Dictionary<string, List<InheritanceLink>> entries = new Dictionary<string, List<InheritanceLink>>();

        public bool AddInheritanceLink(string childClassName, string baseClassName, EAccessSpecifier accessSpecifier)
        {
            List<InheritanceLink> links;
            if (!entries.TryGetValue(childClassName, out links))
            {
                links = new List<InheritanceLink>();
                entries[childClassName] = links;
            }

            bool found = false;
            foreach (var link in links)
            {
                if (link.ClassName == baseClassName)
                { 
                    found = true;
                    break;
                }
            }
            if (found)
                return false;

            links.Add(new InheritanceLink(baseClassName, accessSpecifier));
            return true;
        }
    }
}
