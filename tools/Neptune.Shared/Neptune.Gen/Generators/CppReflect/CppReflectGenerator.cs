using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public abstract class CppReflectGenerator
    {
        protected HeaderFile HeaderFile { get; }
        protected CodeGenCppReflectSettings Settings { get; }

        public CppReflectGenerator(HeaderFile headerFile, CodeGenCppReflectSettings settings)
        {
            HeaderFile = headerFile;
            Settings = settings;
        }

        public abstract void Generate(CodeGenFactory CodeGenFactory);
    }
}
