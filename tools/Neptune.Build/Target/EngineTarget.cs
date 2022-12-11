using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class EngineTarget : ProjectTarget
    {
        private static Version _engineVersion;
        public static ProjectInfo EngineProject => ProjectInfo.Load(Path.Combine(Globals.EngineRoot, "Neptune.nproj"));

        public static Version EngineVersion
        {
            get
            {
                if (_engineVersion == null)
                {
                    _engineVersion = EngineProject.Version;
                    Log.Info(string.Format("Engine build version: {0}", _engineVersion));
                }
                return _engineVersion;
            }
        }

        public override void Init()
        {
            base.Init();

            LinkType = TargetLinkType.Monolithic;

            Modules.Add("Engine");
        }
    }
}
