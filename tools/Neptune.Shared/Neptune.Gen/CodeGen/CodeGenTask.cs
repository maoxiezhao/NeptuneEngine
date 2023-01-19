using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class CodeGenTask : TaskBase
    {
        private CodeGenUnit _codeGenUnit;
        private FileParsingResult _parsingResult;

        public CodeGenTask(CodeGenUnit codeGenUnit, FileParsingResult parsingResult)
        {
            _codeGenUnit = (CodeGenUnit)codeGenUnit.Clone();
            _parsingResult = parsingResult; 
        }

        public override int Run()
        {
            if (DependentTasks.Count == 0)
                return -1;

            ParsingTask parsingTask = null;
            foreach (var task in DependentTasks)
            {
                if (task is ParsingTask)
                {
                    parsingTask = (ParsingTask)task;
                    break;
                }
            }

            if (parsingTask == null)
                return -1;

            if (_parsingResult.Errors.Count > 0)
                return -1;

            if (!_codeGenUnit.GenerateCode(_parsingResult))
                return -1;

            return 0 ;
        }
    }
}
