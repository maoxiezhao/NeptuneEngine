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

        public CodeGenTask(CodeGenUnit codeGenUnit)
        {
            _codeGenUnit = (CodeGenUnit)codeGenUnit.Clone();
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

            FileParsingResult parsingResult = parsingTask.FileParsingResult;
            if (parsingResult.Errors.Count > 0)
                return -1;

            if (!_codeGenUnit.GenerateCode(parsingResult))
                return -1;

            return 0 ;
        }
    }
}
