from platform import architecture
import sys
from enum import Enum
import getopt

# Target platform
class TargetPlatform(Enum):
    Windows = 0

# Target architecture
class TargetArchitecture(Enum):
    AnyCPU = 0
    x86  = 1
    x64  = 2

# Target configuration
class TargetConfiguration(Enum):
    Debug = 0
    Release = 2

TargetPlatformMapping = {
    'win32' : TargetPlatform.Windows
}

ArchitectureMapping = {
    'AnyCPU' : TargetArchitecture.AnyCPU,
    'x64' : TargetArchitecture.x64,
    'x86' : TargetArchitecture.x86,
}

TargetConfigurationMapping = {
    'Debug' : TargetConfiguration.Debug,
    'Release' : TargetConfiguration.Release,
}

# Configuration
class Configuration:
    platform = TargetPlatform.Windows
    architecture = TargetArchitecture.AnyCPU
    configurations = TargetConfiguration.Debug

    # -arch=x64 -configuration=Development -platform=Windows -buildTargets=FlaxEditor -build
    def parse(self, args, profile):
        opts, argv = getopt.getopt(args, '', ['arch=', 'platform=', 'configuration='])
        for op, v in opts:
            if op == '--arch':
                if v in ArchitectureMapping.keys():
                    architecture = ArchitectureMapping[v]             
            elif op == '--configuration':
                if v in TargetConfigurationMapping.keys():
                    architecture = TargetConfigurationMapping[v]     
            pass

        if profile not in TargetPlatformMapping:
            print("Invalid profile ", profile)
            exit(0)

        platform = TargetPlatformMapping[profile]