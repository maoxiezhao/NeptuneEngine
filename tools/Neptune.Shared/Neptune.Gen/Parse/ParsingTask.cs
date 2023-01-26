using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class ParsingTask : TaskBase
    {
        private FileParser _fileParser;
        private HeaderFile _headerFile;

        public ParsingTask(FileParser fileParser, HeaderFile headerFile)
        {
            _fileParser = (FileParser)fileParser.Clone();
            _headerFile = headerFile;
        }

        public override int Run()
        {
            if (_headerFile == null || _headerFile.FilePath.Length == 0)
                return -1;

            _fileParser.Parse(_headerFile, _headerFile.FileParsingResult);
            return _headerFile.FileParsingResult.Errors.Count > 0 ? -1 : 0;
        }
    }
}
