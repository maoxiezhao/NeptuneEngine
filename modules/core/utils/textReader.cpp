#include "textReader.h"

namespace VulkanTest
{
	TextReader::TextReader(const char* buffer_, U32 size_) :
		buffer(buffer_),
		size(size_),
		pos(0),
		line(0),
		cursor(const_cast<char*>(buffer_))
	{
		// separators
		separators.push_back({ '\r', '\n' });
		separators.push_back({'/', '/' });
		separators.push_back({'/', '*' });
		separators.push_back({'\n', 0 });
		separators.push_back({'\t', 0 });
		separators.push_back({' ', 0 });
		separators.push_back({'.', 0 });
		separators.push_back({',', 0 });
		separators.push_back({':', 0 });
		separators.push_back({';', 0 });
		separators.push_back({'+', 0 });
		separators.push_back({'-', 0 });
		separators.push_back({'(', 0 });
		separators.push_back({')', 0 });
		separators.push_back({'!', 0 });
		separators.push_back({'=', 0 });
		separators.push_back({'&', 0 });
		separators.push_back({'%', 0 });
		separators.push_back({'*', 0 });
		separators.push_back({'<', 0 });
		separators.push_back({'>', 0 });
		separators.push_back({'[', 0 });
		separators.push_back({']', 0 });
		separators.push_back({'{', 0 });
		separators.push_back({'}', 0 });

		// whitespaces
		whitespaces.push_back(9);
		whitespaces.push_back(10);
		whitespaces.push_back(11);
		whitespaces.push_back(12);
		whitespaces.push_back(13);
		whitespaces.push_back(32);
	}

	TextReader::~TextReader()
	{
	}

	char TextReader::ReadChar()
	{
		if (pos >= size)
			return 0;
		return MoveForward();
	}

	void TextReader::ReadLine()
	{
		while (pos < size)
		{
			char c = MoveForward();
			if (c == '\n')
				return;
		}
	}

	void TextReader::ReadLine(Token& token)
	{
		token.start = cursor;
		token.length = 0;

		while (pos < size)
		{
			char c = MoveForward();
			if (c == '\n')
				return;

			token.length++;
		}
	}

	void TextReader::ReadToken(Token& token)
	{
		SkipWhiteSpaces();

		token.start = cursor;
		token.length = 0;
		token.separator = {};

		while (pos < size)
		{
			char c = MoveForward();
			char c1 = CurrentChar();

			// Check separator
			for (const auto& separator : separators)
			{
				if (separator.c0 == c && separator.c1 != 0 && separator.c1 == c1)
				{
					token.separator = separator;
					ReadChar();
					return;
				}
				else if (separator.c0 == c && separator.c1 == 0)
				{
					// Skip seprator
					if (token.length == 0)
					{
						token.start = cursor;
						token.length = -1;
						break;
					}

					token.separator = separator;
					return;
				}
			}

			token.length++;
		}
	}

	void TextReader::SkipWhiteSpaces()
	{
		while (pos < size)
		{
			char c = *cursor;
			bool isSpace = false;
			for (auto& space : whitespaces)
			{
				if (space == c)
				{
					MoveForward();
					isSpace = true;
					break;
				}
			}

			if (!isSpace)
				return;
		}
	}

	char TextReader::MoveForward()
	{
		char c = *cursor;
		if (c == '\n')
			line++;
		cursor++;
		pos++;
		return c;
	}

	char TextReader::MoveBack()
	{
		pos--;
		cursor--;
		char c = *cursor;
		if (c == '\n')
			line--;
		return c;
	}
}