#pragma once

#include "core\config.h"

typedef unsigned char byte;
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long long U64;
typedef signed char I8;
typedef signed short I16;
typedef signed int I32;
typedef long long I64;

#ifdef PLATFORM_64BITS
typedef U64 uintptr;
typedef I64 intptr;
#define POINTER_SIZE 8
#else
typedef U32 uintptr;
typedef I32 intptr;
#define POINTER_SIZE 4
#endif

#if PLATFORM_TEXT_IS_CHAR16
typedef char16_t Char;
#else
typedef wchar_t Char;
#endif

#define MIN_U8 ((U8)0x00)
#define	MIN_U16 ((U16)0x0000)
#define	MIN_U32 ((U32)0x00000000)
#define MIN_U64 ((U64)0x0000000000000000)
#define MIN_I8 ((I8)-128)
#define MIN_I16 ((I16)-32768)
#define MIN_I32 -((I32)2147483648)
#define MIN_I64 -((I64)9223372036854775808)
#define MIN_FLOAT -(3.402823466e+38f)
#define MIN_DOUBLE -(1.7976931348623158e+308)
#define MAX_U8 ((U8)0xff)
#define MAX_U16 ((U16)0xffff)
#define MAX_U32 ((U32)0xffffffff)
#define MAX_U64 ((U64)0xffffffffffffffff)
#define MAX_I8 ((I8)127)
#define MAX_I16 ((I16)32767)
#define MAX_I32 ((I32)2147483647)
#define MAX_I64 ((I64)9223372036854775807)
#define MAX_FLOAT (3.402823466e+38f)
#define MAX_DOUBLE (1.7976931348623158e+308)

// Forward declarations
class String;
class StringView;
class StringAnsi;
class StringAnsiView;