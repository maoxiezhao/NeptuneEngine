#ifndef SHADER_INTEROP
#define SHADER_INTEROP

#define SAMPLERSTATE(name, slot) SamplerState name : register(s##slot)

#endif