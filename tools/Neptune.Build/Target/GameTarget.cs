using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class GameProjectTarget : ProjectTarget
    {
        public override void Init()
        {
            base.Init();

            LinkType = TargetLinkType.Modular;
            OutputType = TargetOutputType.Library;
        }
    }
}
