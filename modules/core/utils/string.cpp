#include "string.h"
#include "core\memory\memory.h"
#include "core\platform\platform.h"
#include "math\hash.h"

#include <string>

namespace VulkanTest
{
#define CJING_STRING_ALLOCATOR	CJING_MEMORY_ALLOCATOR_DEFAULT

	size_t StringLength(const char* str)
	{
		return strlen(str);
	}

	bool CopyString(Span<char> dst, const char* source)
	{
		if (!source) {
			return false;
		}

		size_t length = dst.length();
		char* temp = dst.begin();
		while (*source && length > 1)
		{
			*temp = *source;
			length--;
			temp++;
			source++;
		}
		*temp = '\0';
		return *source == '\0';
	}

	bool CopyString(Span<char> dst, Span<const char> source)
	{
		if (dst.length() < 1) return false;
		if (source.length() < 1)
		{
			*dst.pBegin = 0;
			return true;
		}

		U32 len = (U32)dst.length();
		char* mem = dst.pBegin;
		const char* src = source.pBegin;
		while (src != source.pEnd && len > 1)
		{
			*mem = *src;
			len--;
			mem++;
			src++;
		}
		*mem = 0;
		return src == source.pEnd;
	}

	bool CatChar(Span<char> dst, char source)
	{
		size_t length = dst.length();
		size_t size = strlen(dst.begin());
		if (size < length - 1)
		{
			dst[size] = source;
			dst[size + 1] = '\0';
			return true;
		}
		return false;
	}

	bool CatString(Span<char> dst, const char* source)
	{
		// 1. move to the last pos of string
		size_t length = dst.length();
		char* temp = dst.begin();
		while (*temp && length > 0)
		{
			length--;
			temp++;
		}
		// 2. try to copy the source to the last pos
		return CopyString(Span(temp, length), source);
	}

	bool CopyNString(Span<char> dst, const char* source, size_t n)
	{
		if (!source || n <= 0) {
			return false;
		}

		size_t length = dst.length();
		char* temp = dst.begin();
		while (*source && length > 1 && n > 0)
		{
			*temp = *source;
			length--;
			temp++;
			source++;
			n--;
		}

		// if len > 0, need add '\0' in the end
		if (length > 0)
		{
			*temp = '\0';
			return *source == '\0' || n <= 0;
		}
		return false;
	}

	bool CatNString(Span<char> dst, const char* source, size_t n)
	{
		// 1. move to the last pos of string
		size_t length = dst.length();
		char* temp = dst.begin();
		while (*temp && length > 0)
		{
			length--;
			temp++;
		}
		// 2. try to copy the source to the last pos
		return CopyNString(Span(temp, length), source, n);
	}

	int compareString(const char* lhs, const char* rhs, I32 maxCount)
	{
		return strncmp(lhs, rhs, maxCount);
	}

	int compareString(const char* lhs, const char* rhs)
	{
		return strcmp(lhs, rhs);
	}

	bool EqualString(const char* lhs, const char* rhs)
	{
		return strcmp(lhs, rhs) == 0;
	}

	bool EqualString(Span<const char> lhs, Span<const char> rhs)
	{
		if (rhs.length() != lhs.length()) return false;
		return strncmp(lhs.begin(), rhs.begin(), lhs.length()) == 0;
	}

	bool EqualIStrings(const char* lhs, const char* rhs)
	{
#ifdef _WIN32
		return _stricmp(lhs, rhs) == 0;
#else
		return strcasecmp(lhs, rhs) == 0;
#endif
	}

	int FindStringChar(const char* str, const char c, int pos)
	{
		const char* ret = strchr(str + pos, c);
		return ret != nullptr ? (int)(ret - str) : -1;
	}

	int ReverseFindChar(const char* str, const char c)
	{
		const char* ret = strrchr(str, c);
		return ret != nullptr ? (int)(ret - str) : -1;
	}

	int FindSubstring(const char* str, const char* substr, int pos)
	{
		const char* ret = strstr(str + pos, substr);
		return ret != nullptr ? (int)(ret - str) : -1;
	}

	int ReverseFindSubstring(const char* str, const char* substr)
	{
		return (int)std::string(str).rfind(substr);
	}

	void ReverseString(char* str, size_t n)
	{
		char* beg = str;
		char* end = str + n - 1;
		while (beg < end)
		{
			char tmp = *beg;
			*beg = *end;
			*end = tmp;
			++beg;
			--end;
		}
	}

	bool StartsWith(const char* str, const char* substr)
	{
		const char* lhs = str;
		const char* rhs = substr;
		while (*rhs && *lhs && *lhs == *rhs) 
		{
			++lhs;
			++rhs;
		}
		return *rhs == 0;
	}

	bool EndsWith(const char* str, const char* substr)
	{
		int len = (int)StringLength(str);
		int len2 = (int)StringLength(substr);
		if (len2 > len)
			return false;
		return EqualString(str + len - len2, substr);
	}

	bool FromCString(Span<const char> input, I32& value)
	{
		U32 length = input.length();
		if (length <= 0)
			return false;
		
		const char* c = input.begin();
		value = 0;
		if (*c == '-')
		{
			++c;
			--length;
			if (!length)
				return false;
		}
		while (length && *c >= '0' && *c <= '9')
		{
			value *= 10;
			value += *c - '0';
			++c;
			--length;
		}

		if (input[0] == '-')
			value = -value;

		return c == input.end();
	}

	bool FromCString(Span<const char> input, U32& value)
	{
		U32 length = input.length();
		if (length <= 0)
			return false;

		const char* c = input.begin();
		value = 0;
		if (*c == '-')
			return false;

		while (length && *c >= '0' && *c <= '9')
		{
			value *= 10;
			value += *c - '0';
			++c;
			--length;
		}
		return c == input.end();
	}

	bool FromHexString(Span<const char> input, U32& value)
	{
		U32 length = input.length();
		if (length <= 0)
			return false;

		const char* c = input.begin();
		value = 0;
		if (*c == '-')
			return false;

		if (*c == '0' && *(c + 1) == 'x')
			c += 2;

		while (length)
		{
			I32 v = *c - '0';
			if (v < 0 || v > 9)
			{
				v = MakeLowercase(*c) - 'a' + 10;
				if (v < 10 || v > 15)
					break;
			}

			value = 16 * value + v;
			++c;
			--length;
		}
		return c == input.end();
	}

	bool ToCString(I32 value, Span<char> output)
	{
		char* c = output.begin();
		U32 length = (U32)output.length();
		if (length > 0)
		{
			if (value < 0)
			{
				value = -value;
				--length;
				*c = '-';
				++c;
			}
			return ToCString((U32)value, Span(c, length));
		}
		return false;
	}

	bool ToCString(I64 value, Span<char> output)
	{
		char* c = output.begin();
		U32 length = (U32)output.length();
		if (length > 0)
		{
			if (value < 0)
			{
				value = -value;
				--length;
				*c = '-';
				++c;
			}
			return ToCString((U64)value, Span(c, length));
		}
		return false;
	}

	bool ToCString(U32 value, Span<char> output)
	{
		char* c = output.begin();
		char* num_start = output.begin();
		U32 length = (U32)output.length();
		if (length > 0)
		{
			if (value == 0)
			{
				if (length == 1)
					return false;

				*c = '0';
				*(c + 1) = 0;
				return true;
			}
			while (value > 0 && length > 1)
			{
				*c = value % 10 + '0';
				value = value / 10;
				--length;
				++c;
			}
			if (length > 0)
			{
				ReverseString(num_start, (int)(c - num_start));
				*c = 0;
				return true;
			}
		}
		return false;
	}

	bool ToCString(U64 value, Span<char> output)
	{
		char* c = output.begin();
		char* num_start = output.begin();
		U32 length = (U32)output.length();
		if (length > 0)
		{
			if (value == 0)
			{
				if (length == 1)
					return false;

				*c = '0';
				*(c + 1) = 0;
				return true;
			}
			while (value > 0 && length > 1)
			{
				*c = value % 10 + '0';
				value = value / 10;
				--length;
				++c;
			}
			if (length > 0)
			{
				ReverseString(num_start, (int)(c - num_start));
				*c = 0;
				return true;
			}
		}
		return false;
	}


	static bool Increment(const char* output, char* end, bool is_space_after)
	{
		char carry = 1;
		char* c = end;
		while (c >= output)
		{
			if (*c == '.')
			{
				--c;
			}
			*c += carry;
			if (*c > '9')
			{
				*c = '0';
				carry = 1;
			}
			else
			{
				carry = 0;
				break;
			}
			--c;
		}
		if (carry && is_space_after)
		{
			char* c = end + 1; // including '\0' at the end of the String
			while (c >= output)
			{
				*(c + 1) = *c;
				--c;
			}
			++c;
			*c = '1';
			return true;
		}
		return !carry;
	}

	bool ToCString(F32 value, Span<char> out, int afterPoint)
	{
		char* output = out.begin();
		U32 length = (U32)out.length();
		if (length < 2)
		{
			return false;
		}
		if (value < 0)
		{
			*output = '-';
			++output;
			value = -value;
			--length;
		}
		// int part
		int exponent = value == 0 ? 0 : (int)log10(value);
		double num = value;
		char* c = output;
		if (num  < 1 && num > -1 && length > 1)
		{
			*c = '0';
			++c;
			--length;
		}
		else
		{
			while ((num >= 1 || exponent >= 0) && length > 1)
			{
				const double power = pow(10.0, (double)exponent);
				char digit = (char)floor(num / power);
				num -= digit * power;
				*c = digit + '0';
				--exponent;
				--length;
				++c;
			}
		}
		// decimal part
		double dec_part = num;
		if (length > 1 && afterPoint > 0)
		{
			*c = '.';
			++c;
			--length;
		}
		else if (length > 0 && afterPoint == 0)
		{
			*c = 0;
			return true;
		}
		else
		{
			return false;
		}
		while (length > 1 && afterPoint > 0)
		{
			dec_part *= 10;
			char tmp = (char)dec_part;
			*c = tmp + '0';
			dec_part -= tmp;
			++c;
			--length;
			--afterPoint;
		}
		*c = 0;
		if ((int)(dec_part + 0.5f))
			Increment(output, c - 1, length > 1);

		return true;
	}

	bool ToCString(F64 value, Span<char> out, int afterPoint)
	{
		char* output = out.begin();
		U32 length = (U32)out.length();
		if (length < 2)
		{
			return false;
		}
		if (value < 0)
		{
			*output = '-';
			++output;
			value = -value;
			--length;
		}
		// int part
		int exponent = value == 0 ? 0 : (int)log10(value);
		double num = value;
		char* c = output;
		if (num  < 1 && num > -1 && length > 1)
		{
			*c = '0';
			++c;
			--length;
		}
		else
		{
			while ((num >= 1 || exponent >= 0) && length > 1)
			{
				double power = (double)pow(10, exponent);
				char digit = (char)floor(num / power);
				num -= digit * power;
				*c = digit + '0';
				--exponent;
				--length;
				++c;
			}
		}
		// decimal part
		double dec_part = num;
		if (length > 1 && afterPoint > 0)
		{
			*c = '.';
			++c;
			--length;
		}
		else if (length > 0 && afterPoint == 0)
		{
			*c = 0;
			return true;
		}
		else
		{
			return false;
		}
		while (length > 1 && afterPoint > 0)
		{
			dec_part *= 10;
			char tmp = (char)dec_part;
			*c = tmp + '0';
			dec_part -= tmp;
			++c;
			--length;
			--afterPoint;
		}
		*c = 0;
		if ((int)(dec_part + 0.5f))
			Increment(output, c - 1, length > 1);

		return true;
	}

	char MakeLowercase(char c)
	{
		return c >= 'A' && c <= 'Z' ? c - ('A' - 'a') : c;
	}

	bool MakeLowercase(Span<char> dst, const char* src)
	{
		char* destination = dst.begin();
		U32 length = (U32)dst.length();
		if (!src)
		{
			return false;
		}

		while (*src && length)
		{
			*destination = MakeLowercase(*src);
			--length;
			++destination;
			++src;
		}
		if (length > 0)
		{
			*destination = 0;
			return true;
		}
		return false;
	}

	bool MakeLowercase(Span<char> dst, Span<const char> src)
	{
		char* destination = dst.begin();
		if (src.length() + 1 > dst.length()) 
			return false;

		const char* source = src.begin();
		while (source != src.end())
		{
			*destination = MakeLowercase(*source);
			++destination;
			++source;
		}
		*destination = 0;
		return true;
	}

	U32 HashFunc(U32 Input, const String& Data)
	{
		return SDBHash(Input, Data.data(), Data.size());
	}

	U64 HashFunc(U64 Input, const String& Data)
	{
		return FNV1aHash(Input, Data.data(), Data.size());
	}

	String WStringToString(const WString& wstr)
	{
#ifdef CJING3D_PLATFORM_WIN32
		if (wstr.empty()) return String();
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		String strTo(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
#else
		ASSERT(false);
#endif
	}

	WString StringToWString(const String& str)
	{
#ifdef CJING3D_PLATFORM_WIN32
		if (str.empty()) return std::wstring();
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
		std::wstring wstrTo(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
		return wstrTo;
#else
		ASSERT(false);
#endif
	}

#if (CJING_MEMORY_ALLOCATOR == CJING_MEMORY_ALLOCATOR_DEFAULT)
	DefaultAllocator gStringAllocator;
#endif

	const String String::EMPTY;

	String::String()
	{
		stringSize = 0;
		smallData[0] = '\0';
	}

	String::String(const char c)
	{
		stringSize = 1;
		smallData[0] = c;
		smallData[1] = '\0';
	}

	String::String(const char* str)
	{
		stringSize = StringLength(str);
		if (stringSize < BUFFER_MINIMUN_SIZE)
		{
			Memory::Memcpy(smallData, str, stringSize + 1);
		}
		else
		{
	
			bigData = (char*)CJING_ALLOCATOR_MALLOC(gStringAllocator, stringSize + 1);
			Memory::Memcpy(bigData, str, stringSize + 1);
		}
	}

	String::String(size_t size, char initChar)
	{
		resize(size);
		Memory::Memset(data(), initChar, (int)size);
	}

	String::String(const String& rhs)
	{
		stringSize = 0;
		*this = Span(rhs.c_str(), rhs.length());
	}

	String::String(String&& rhs)
	{
		if (isSmall())
		{
			Memory::Memcpy(smallData, rhs.smallData, sizeof(smallData));
			rhs.smallData[0] = '\0';
		}
		else
		{
			bigData = rhs.bigData;
			rhs.bigData = nullptr;
		}
		stringSize = rhs.stringSize;
		rhs.stringSize = 0;
	}

	String::String(const std::string& str)
	{
		stringSize = 0;
		*this = Span(str.c_str(), str.length());
	}

	String::String(Span<const char> str)
	{
		stringSize = 0;
		*this = str;
	}

	String::String(const char* str, size_t pos, size_t len)
	{
		stringSize = 0;
		*this = Span(str + pos, len);
	}

	String::String(const char* begin, const char* end)
	{
		stringSize = 0;
		*this = Span(begin, end);
	}

	String::~String()
	{
		if (!isSmall()) {
			CJING_ALLOCATOR_FREE(gStringAllocator, bigData);
		}
	}

	String& String::operator=(const String& rhs)
	{
		if (&rhs == this) {
			return *this;
		}
		*this = Span(rhs.c_str(), rhs.length());
		return *this;
	}

	String& String::operator=(String&& rhs)
	{
		if (&rhs == this) {
			return *this;
		}

		if (!isSmall()) 
		{
			CJING_ALLOCATOR_FREE(gStringAllocator, bigData);
		}

		if (rhs.isSmall())
		{
			Memory::Memcpy(smallData, rhs.smallData, sizeof(smallData));
			rhs.smallData[0] = '\0';
		}
		else
		{
			bigData = rhs.bigData;
			rhs.bigData = nullptr;
		}

		stringSize = rhs.stringSize;
		rhs.stringSize = 0;
		return *this;
	}

	String& String::operator=(Span<const char> str)
	{
		if (!isSmall()) 
		{
			CJING_ALLOCATOR_FREE(gStringAllocator, bigData);
		}

		if (str.length() < BUFFER_MINIMUN_SIZE)
		{
			Memory::Memcpy(smallData, str.data(), str.length());
			smallData[str.length()] = '\0';
		}
		else
		{
			size_t size = str.length() + 1;
			if (size == 35)
				int a = 0;
			bigData = (char*)CJING_ALLOCATOR_MALLOC(gStringAllocator, str.length() + 1);
			Memory::Memcpy(bigData, str.data(), str.length());
			bigData[str.length()] = '\0';
		}

		stringSize = str.length();
		return *this;
	}

	String& String::operator=(const char* str)
	{
		*this = Span(str, StringLength(str));
		return *this;
	}

	String& String::operator=(const std::string& str)
	{
		*this = Span(str.c_str(), str.length());
		return *this;
	}

	void String::resize(size_t size)
	{
		if (size <= 0) {
			return;
		}

		// need to keep original data
		if (isSmall())
		{
			if (size < BUFFER_MINIMUN_SIZE)
			{
				smallData[size] = '\0';
			}
			else
			{
				char* tmp = (char*)CJING_ALLOCATOR_MALLOC(gStringAllocator, size + 1);
				Memory::Memcpy(tmp, smallData, stringSize + 1);
				bigData = tmp;
			}
			stringSize = size;
		}
		else
		{
			if (size < BUFFER_MINIMUN_SIZE)
			{
				char* tmp = bigData;
				memcpy(smallData, tmp, BUFFER_MINIMUN_SIZE - 1);
				smallData[size] = '\0';
				gStringAllocator.Free(tmp);
			}
			else
			{
				bigData = (char*)CJING_ALLOCATOR_REMALLOC(gStringAllocator, bigData, size + 1);
				bigData[size] = '\0';
			}
			stringSize = size;
		}
	}

	char& String::operator[](size_t index)
	{
		ASSERT(index >= 0 && index < stringSize);
		return isSmall() ? smallData[index] : bigData[index];
	}

	const char& String::operator[](size_t index) const
	{
		ASSERT(index >= 0 && index < stringSize);
		return isSmall() ? smallData[index] : bigData[index];
	}

	bool String::operator!=(const String& rhs) const
	{
		return !EqualString(c_str(), rhs.c_str());
	}

	bool String::operator!=(const char* rhs) const
	{
		return !EqualString(c_str(), rhs);
	}

	bool String::operator==(const String& rhs) const
	{
		return EqualString(c_str(), rhs.c_str());
	}

	bool String::operator==(const char* rhs) const
	{
		return EqualString(c_str(), rhs);
	}

	bool String::operator<(const String& rhs) const
	{
		return compareString(c_str(), rhs.c_str()) < 0;
	}

	bool String::operator>(const String& rhs) const
	{
		return compareString(c_str(), rhs.c_str()) > 0;
	}

	String& String::append(Span<const char> value)
	{
		if (value.length() == 0) {
			return *this;
		}

		size_t oldSize = stringSize;
		resize(oldSize + value.length());
		Memory::Memcpy(data() + oldSize, value.data(), value.length());
		data()[oldSize + value.length()] = '\0';
		return *this;
	}

	String& String::append(const char* value)
	{
		const size_t len = StringLength(value);
		if (len == 0) {
			return *this;
		}

		size_t oldSize = stringSize;
		resize(oldSize + len);
		Memory::Memcpy(data() + oldSize, value, len + 1);
		return *this;
	}

	String& String::append(char value)
	{
		return append(Span(&value, 1));
	}

	String& String::append(char* value)
	{
		return append((const char*)value);
	}

	String& String::append(const std::string& value)
	{
		return append(value.c_str());
	}

	String String::substr(size_t pos, int length)const
	{
		length = length < 0 ? (int)(stringSize - pos) : std::min((int)stringSize - (int)pos, length);
		return String(c_str(), pos, std::max(0, length));
	}

	void String::insert(size_t pos, const char* value)
	{
		const size_t oldSize = stringSize;
		size_t len = StringLength(value);
		resize(oldSize + len);

		char* temp = data();
		Memory::Memmove(temp + pos + len, temp + pos, oldSize - pos);
		Memory::Memcpy(temp + pos, value, len);
	}

	void String::erase(size_t pos)
	{
		if (pos >= stringSize) {
			return;
		}

		char* temp = data();
		Memory::Memmove(temp + pos, temp + pos + 1, stringSize - pos - 1);
		stringSize--;
		temp[stringSize] = '\0';
	}

	void String::erase(size_t pos, size_t count)
	{
		if (pos >= stringSize || count <= 0) {
			return;
		}
		count = std::min(count, stringSize - pos);

		char* temp = data();
		Memory::Memmove(temp + pos, temp + pos + count, stringSize - pos - count);
		stringSize -= count;
		temp[stringSize] = '\0';
	}

	int String::find(const char* str, size_t pos)const
	{
		pos = std::min(stringSize - 1, pos);
		return FindSubstring(c_str(), str, (int)pos);
	}

	int String::find(const char c, size_t pos)const
	{
		pos = std::min(stringSize - 1, pos);
		return FindStringChar(c_str(), c, (int)pos);
	}

	int String::find_last_of(const char* str)const
	{
		return ReverseFindSubstring(c_str(), str);
	}

	int String::find_last_of(const char c)const
	{
		return ReverseFindChar(c_str(), c);
	}

	char String::back() const
	{
		return stringSize > 0 ? c_str()[stringSize - 1] : '\0';
	}

	void String::clear()
	{
		if (!isSmall()) 
		{
			CJING_ALLOCATOR_FREE(gStringAllocator, bigData);
		}

		stringSize = 0;
		smallData[0] = '\0';
	}

	void String::replace(size_t pos, size_t len, const char* str)
	{
		if (pos >= stringSize ||
			len > stringSize - pos) {
			return;
		}

		const size_t srcLen = StringLength(str);
		if (srcLen == len)
		{
			// if same length, just copy new str to dest
			Memory::Memcpy(data() + pos, str, len);
		}
		else if (pos + len < stringSize)
		{
			const size_t oldSize = stringSize;
			if (srcLen > len)
			{
				resize(oldSize + (srcLen - len));
				char* temp = data();
				Memory::Memmove(temp + pos + srcLen, temp + pos + len, oldSize - pos - len);
				Memory::Memcpy(temp + pos, str, srcLen);
			}
			else
			{
				char* temp = data();
				Memory::Memmove(temp + pos + srcLen, temp + pos + len, oldSize - pos - len);
				resize(oldSize + (srcLen - len));
				Memory::Memcpy(data() + pos, str, srcLen);
			}
		}
		else
		{
			resize(stringSize + (srcLen - len));
			Memory::Memcpy(data() + pos, str, srcLen);
		}
	}

	Span<char> String::toSpan()
	{
		return Span<char>(data(), stringSize);
	}

	Span<const char> String::toSpan() const
	{
		return Span<const char>(c_str(), stringSize);
	}

	std::string String::toString() const
	{
		return std::string(c_str());
	}

	bool String::isSmall() const
	{
		return stringSize < BUFFER_MINIMUN_SIZE;
	}
}

