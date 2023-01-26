using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class FileParsingResult : ParsingResultBase
    {
        public List<string> IncludedHeaders = new List<string>();

        /// <summary>
        /// All enums contained directly under file level.
        /// </summary>
        public List<EnumInfo> Enums = new List<EnumInfo>();

        /// <summary>
        /// All structs contained directly under file level.
        /// </summary>
        public List<StructClassInfo> Structs = new List<StructClassInfo>();

        /// <summary>
        /// All classes contained directly under file level.
        /// </summary>
        public List<StructClassInfo> Classes = new List<StructClassInfo>();

        /// <summary>
        /// Structure containing the whole struct/class hierarchy linked to parsed structs/classes.
        /// </summary>
        public StructClassTree StructClassTree = new StructClassTree();
    }
}
