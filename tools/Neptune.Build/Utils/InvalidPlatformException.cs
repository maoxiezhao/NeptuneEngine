using System;

namespace Neptune.Build
{
    public class InvalidPlatformException : Exception
    {
        public InvalidPlatformException(TargetPlatform platform)
        : base(string.Format("Unknown platform {0}.", platform))
        {
        }

        public InvalidPlatformException(TargetPlatform platform, string message)
        : base(string.Format("Unknown platform {0}. " + message, platform))
        {
        }
    }
}
