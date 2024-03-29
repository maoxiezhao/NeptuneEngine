﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build
{
    public class VisualStudioProject : Project
    {
        public Guid ProjectGuid;

        public override string Path
        {
            get => base.Path;
            set
            {
                base.Path = value;

                if (ProjectGuid == Guid.Empty)
                {
                    ProjectGuid = VSProjectGenerator.GetProjectGuid(Path);
                }
            }
        }

        public virtual Guid ProjectTypeGuid
        {
            get
            {
                switch (TargetType)
                {
                    case TargetType.Cpp: return ProjectTypeGuids.WindowsVisualCpp;
                    case TargetType.DotNet: return ProjectTypeGuids.WindowsCSharp;
                    default: throw new ArgumentOutOfRangeException();
                }
            }
        }

    }
}
