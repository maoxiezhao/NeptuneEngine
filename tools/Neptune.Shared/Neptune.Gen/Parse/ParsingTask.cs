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
        private string _filePath;
        public string FilePath { get { return _filePath; } }
        public FileParsingResult FileParsingResult { get; set; } = new FileParsingResult();

        public ParsingTask(FileParser fileParser, string filePath)
        {
            _fileParser = (FileParser)fileParser.Clone();
            _filePath = filePath;
        }

        public override int Run()
        {
            if (_filePath.Length == 0)
                return -1;

            _fileParser.Parse(_filePath, FileParsingResult);
            return FileParsingResult.Errors.Count > 0 ? -1 : 0;
        }
    }
}
