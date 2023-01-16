using ClangSharp;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class ApiTokens
    {
        public string Class = "API_CLASS";
        public string Struct = "API_STRUCT";
        public string Variable = "API_FIELD";
        public string Field = "API_FIELD";
        public string Function = "API_FUNCTION";
        public string Method = "API_METHOD";
        public string Enum = "API_ENUM";
        public string EnumValue = "API_ENUMVALUE";

        public List<string> GetSearchTags()
        {
            return new List<string>() {
                Enum,
                Class,
                Struct,
                Variable,
                Field,
                Function,
                Method,
                Enum,
                EnumValue
            };
        }
    }
}
