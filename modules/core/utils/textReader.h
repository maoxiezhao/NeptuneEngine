#pragma once

#include "string.h"
#include "stringID.h"
#include "core\collections\Array.h"

namespace VulkanTest
{
	class VULKAN_TEST_API TextReader
	{
	public:
        struct Separator
        {
        public:
            char c0 = 0;
            char c1 = 0;

			Separator() = default;
            Separator(char c0_) : c0(c0_), c1(0) {}
            Separator(char c0_, char c1_) : c0(c0_), c1(c1_) {}

            bool IsWhiteSpace() const
            {
                return *this == Separator('\r', '\n')
                    || *this == Separator('\n')
                    || *this == Separator('\t')
                    || *this == Separator(' ');
            }

            bool operator==(const Separator& other) const {
                return c0 == other.c0 && c1 == other.c1;
            }

            bool operator!=(const Separator& other) const {
                return c0 != other.c0 || c1 != other.c1;
            }
        };

		struct Token
		{
			const char* start;
			I32 length;
			Separator separator;
	
			Token() :
				start(nullptr),
				length(0)
			{}

			Token(const char* text) :
				start(text),
				length((I32)StringLength(text))
			{
			}

			Token(const char* text, I32 length_) : 
				start(text),
				length(length_)
			{
			}

			explicit Token(const String& string) :
				start(string.c_str()),
				length((I32)string.length())
			{
			}

			Span<const char> ToSpan()const {
				return Span(start, length);
			}

			String ToString()const {
				return String(start, start + length);
			}

			bool operator==(const char* str) const {
				return *this == Token(str);
			}

			bool operator==(const Token& other) const {
				return length == other.length && EqualString(ToSpan(), other.ToSpan());
			}

			bool operator!=(const Token& other) const {
				return length != other.length || !EqualString(ToSpan(), other.ToSpan());
			}
		};

		TextReader(const char* buffer_, U32 size_);
		~TextReader();

		bool IsEmpty()const {
			return buffer != nullptr && size > 0;
		}
		bool CanRead()const {
			return pos < size;
		}
		U32 CurrentLine()const {
			return line;
		}
		char CurrentChar()const {
			return *cursor;
		}

		char ReadChar();
		void ReadLine();
		void ReadLine(Token& token);
		void ReadToken(Token& token);
		void SkipWhiteSpaces();

	private:
		char MoveForward();
		char MoveBack();

		const char* buffer;
		char* cursor;
		U32 size;
		U32 pos;
		U32 line;

		Array<Separator> separators;
		Array<char> whitespaces;
	};
}