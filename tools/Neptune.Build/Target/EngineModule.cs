using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class EngineModule : Module
    {
        public override void Setup(BuildOptions options)
        {
            base.Setup(options);

            if (Name != "Core")
            {
                // All Engine modules include Core module
                options.PrivateDependencies.Add("Core");
            }

            BinaryModuleName = "NeptuneEngine";
        }
    }
}
