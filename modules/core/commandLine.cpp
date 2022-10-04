#include "commandLine.h"
#include "core\collections\array.h"

namespace VulkanTest
{
	CommandLine::Options CommandLine::options;

    bool IsWhitespace(char c)
    {
        return isspace(c) != 0;
    }

    bool ParseArg(char* ptr, char*& start, char*& end)
    {
        while (*ptr && IsWhitespace(*ptr))
            ptr++;

        bool isInComma = false;
        bool isUglyQuote = false;
        start = ptr;
        while (*ptr)
        {
            char c = *ptr;
            if (IsWhitespace(c) && !isInComma)
            {
                end = ptr;
                return true;
            }
            else if (c == '\"' || c == '\'')
            {
                if (isInComma)
                {
                    // End comma
                    end = ptr;
                    if (isUglyQuote)
                    {
                        ptr += 2;
                        end -= 2;
                    }
                    return true;
                }
                else
                {
                    // Special case (eg. Visual Studio Code adds soo many quotes to the args with spaces)
                    isUglyQuote = strncmp(ptr, "\"\\\\\"", 4) == 0;
                    if (isUglyQuote)
                    {
                        ptr += 3;
                    }

                    // Start comma
                    isInComma = true;
                    start = ptr + 1;
                }
            }

            ptr++;
        }
        return false;
    }


	bool CommandLine::Parse(const char* cmdLine)
	{
        auto length = StringLength(cmdLine);
        if (length <= 0)
            return false;

        Array<char> buffer;
        buffer.resize(length + 2);
        memcpy(buffer.data(), cmdLine, sizeof(char) * length);
        buffer[length++] = ' ';
        buffer[length++] = 0;
        char* end = buffer.data() + length;

        char* pos;
        char* argStart;
        char* argEnd;
#ifdef CJING3D_EDITOR
		auto posIndex = FindSubstring(buffer.data(), "-project", 0);
		if (posIndex >= 0)
		{
            pos = buffer.data() + posIndex;
            int len = ARRAYSIZE("-project") - 1;
            if (!ParseArg(pos + len, argStart, argEnd))
            {
                std::cout << "Failed to parse argument." << std::endl;
                return true;
            }
            options.projectPath = String((const char*)argStart, 0, static_cast<I32>(argEnd - argStart));
            len = static_cast<I32>((argEnd - pos) + 1);
            memcpy(pos, pos + len, (end - pos - len) * 2);
            * (end - len) = 0;
            end -= len;
		}
#endif
		return true;
	}
}