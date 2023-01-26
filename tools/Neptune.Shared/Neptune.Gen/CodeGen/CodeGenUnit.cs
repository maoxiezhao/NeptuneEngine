using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public abstract class CodeGenUnit
    {
        protected CodeGenSettings _settings;
        protected CodeGenFactory _codeGenFactory;

        public CodeGenUnit(CodeGenFactory factory, CodeGenSettings settings)
        {
            _codeGenFactory = factory;
            _settings = settings;
        }

        public abstract void GenerateCode(HeaderFile headerFile);
    }
}
