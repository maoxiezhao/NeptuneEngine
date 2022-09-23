#pragma once

#include "core\common.h"
#include "core\utils\crc32.h"

#include <stdarg.h>
#include <string_view>
#include <string>

#define USE_CUSTOM_STRING

namespace VulkanTest
{
	size_t StringLength(const char* str);
	bool CopyString(Span<char> dst, const char* source);
	bool CopyString(Span<char> dst, Span<const char> source);
	bool CatChar(Span<char> dst, char source);
	bool CatString(Span<char> dst, const char* source);
	bool CopyNString(Span<char> dst, const char* source, size_t n);
	bool CatNString(Span<char> dst, const char* source, size_t n);
	int  compareString(const char* lhs, const char* rhs);	// CompareString macro confict
	bool EqualString(const char* lhs, const char* rhs);
	bool EqualString(Span<const char> lhs, Span<const char> rhs);
	bool EqualIStrings(const char* lhs, const char* rhs);
	int  FindStringChar(const char* str, const char c, int pos);
	int  ReverseFindChar(const char* str, const char c);
	int  FindSubstring(const char* str, const char* substr, int pos);
	int  ReverseFindSubstring(const char* str, const char* substr);
	void ReverseString(char* str, size_t n);
	bool StartsWith(const char* str, const char* substr);
	bool EndsWith(const char* str, const char* substr);
	bool FromCString(Span<const char> input, I32& value);
	bool FromCString(Span<const char> input, U32& value);
	bool FromHexString(Span<const char> input, U32& value);
	bool ToCString(I32 value, Span<char> output);
	bool ToCString(I64 value, Span<char> output);
	bool ToCString(U32 value, Span<char> output);
	bool ToCString(U64 value, Span<char> output);
	bool ToCString(F32 value, Span<char> output, int afterPoint);
	bool ToCString(F64 value, Span<char> output, int afterPoint);
	char MakeLowercase(char c);
	bool MakeLowercase(Span<char> dst, const char* src);
	bool MakeLowercase(Span<char> dst, Span<const char> src);

	template<size_t N>
	bool CopyString(char(&destination)[N], const char* source)
	{
		return CopyString(Span(destination, N), source);
	}

	template<size_t N>
	bool CatString(char(&destination)[N], const char* source)
	{
		return CatString(Span(destination, N), source);
	}

	// Fixed size string 
	template<size_t N>
	class VULKAN_TEST_API StaticString
	{
	public:
		char data[N];
		
		StaticString()
		{
			data[0] = '\0';
		}

		StaticString(const char* str)
		{
			data[0] = '\0';
			CopyString(data, str);
		}

		template<typename... Args>
		StaticString(Args... args)
		{
			data[0] = '\0';
			(append(args), ...);
		}

		template<size_t RHS_N>
		void append(const StaticString<RHS_N>& rhs) {
			CatString(data, rhs.data);
		}

		void append(const char* str) {
			CatString(data, str);
		}

		void append(char* str) {
			CatString(data, str);
		}

		void append(Span<const char*> str) {
			CatString(data, str.data());
		}

		void append(char c) 
		{
			char temp[2] = { c, 0 };
			CatString(data, temp);
		}

		void append(const Span<char>& rhs)
		{
			CatNString(data, rhs.data(), rhs.length());
		}

		void append(const std::string& rhs)
		{
			CatString(data, rhs.data());
		}

		template <typename T>
		void append(T v)
		{
			size_t len = StringLength(data);
			ToCString(v, Span(data + len, U32(N - len)));
		}

		bool empty()const {
			return data[0] == '\0';
		}

		char* c_str() { 
			return data;
		}

		const char* c_str() const { 
			return data;
		}

		void clear() {
			data[0] = '\0';
		}

		const char back()const {
			return data[StringLength(data) - 1];
		}

		size_t size()const {
			return N;
		}

		size_t length()const {
			return StringLength(data);
		}

		template <int Size> 
		StaticString& operator<<(StaticString<Size>& value)
		{
			append(value);
			return *this;
		}

		template <typename T> 
		StaticString& operator<<(T value)
		{
			append(value);
			return *this;
		}

		void operator=(const char* str) {
			CopyString(data, str);
		}

		bool operator<(const char* str) const {
			return CompareString(data, str) < 0;
		}

		bool operator==(const char* str) const {
			return EqualString(data, str);
		}

		bool operator!=(const char* str) const {
			return !EqualString(data, str);
		}

		StaticString<N> operator+(const char* str) {
			return StaticString<N>(data, str);
		}

		StaticString<N>& operator+=(const char* str) {
			append(str);
			return *this;
		}

		operator const char* () const { 
			return data;
		}

		char& operator[](size_t index)
		{
			assert(index >= 0 && index < N);
			return data[index];
		}

		const char& operator[](size_t index) const
		{
			assert(index >= 0 && index < N);
			return data[index];
		}

		Span<char> toSpan() {
			return Span(data);
		}

		Span<const char> toSpan()const {
			return Span(data);
		}

		StaticString& Sprintf(const char* format, ...)
		{
			va_list args;
			va_start(args, format);
			vsnprintf(data, N, format, args);
			va_end(args);
			return *this;
		}

		StaticString& Sprintfv(const char* format, va_list args)
		{
			vsprintf_s(data, N, format, args);
			return *this;
		}
	};

	using String16  = StaticString<16>;
	using String32  = StaticString<32>;
	using String64  = StaticString<64>;
	using String128 = StaticString<128>;

#ifndef USE_CUSTOM_STRING
	using String = std::string;
	using StringView = std::string_view;
#else
	class VULKAN_TEST_API String
	{
	public:
		String();
		String(const char c);
		String(const char* str);
		String(size_t size, char initChar);
		String(const String& rhs);
		String(String&& rhs);
		String(const std::string& str);
		String(Span<const char> str);
		String(const char* str, size_t pos, size_t len);
		String(const char* begin, const char* end);
		~String();

		String& operator=(const String& rhs);
		String& operator=(String&& rhs);
		String& operator=(Span<const char> str);
		String& operator=(const char* str);
		String& operator=(const std::string& str);

		const char* c_str() const { return isSmall() ? smallData : bigData; }
		bool  empty() const { return c_str() == nullptr || c_str()[0] == '\0'; }
		char* data() { return isSmall() ? smallData : bigData; }
		const char* data()const { return isSmall() ? smallData : bigData; }
		size_t length()const { return stringSize; }
		size_t size()const { return stringSize; }
		std::string toString()const;

		char& operator[](size_t index);
		const char& operator[](size_t index) const;

		bool operator!=(const String& rhs) const;
		bool operator!=(const char* rhs) const;
		bool operator==(const String& rhs) const;
		bool operator==(const char* rhs) const;
		bool operator<(const String& rhs) const;
		bool operator>(const String& rhs) const;

		String& operator+=(const char* str) {
			append(str);
			return *this;
		}
		String& operator+=(const char c) {
			append(c);
			return *this;
		}
		String operator+(const char* str) {
			return String(*this).append(str);
		}
		String operator+(const char c) {
			return String(*this).append(c);
		}
		operator const char* () const {
			return c_str();
		}

		String& append(Span<const char> value);
		String& append(char value);
		String& append(char* value);
		String& append(const char* value);
		String& append(const std::string& value);

		void   resize(size_t size);
		String substr(size_t pos, int length = -1)const;
		void   insert(size_t pos, const char* value);
		void   erase(size_t pos);
		void   erase(size_t pos, size_t count);
		int    find(const char* str, size_t pos = 0)const;
		int    find(const char c, size_t pos = 0)const;
		int    find_last_of(const char* str)const;
		int    find_last_of(const char c)const;
		char   back()const;
		void   clear();
		void   replace(size_t pos, size_t len, const char* str);

		Span<char> toSpan();
		Span<const char> toSpan()const;

		char* begin() {
			return data();
		}
		char* end() {
			return data() + stringSize;
		}

		static const int npos = -1;
		static const String EMPTY;

	private:
		bool isSmall()const;

		// mininum buffer size
		static const size_t BUFFER_MINIMUN_SIZE = 16;
		size_t stringSize = 0;
		union {
			char* bigData = nullptr;
			char  smallData[BUFFER_MINIMUN_SIZE];
		};
	};

	// hash func
	U32 HashFunc(U32 Input, const String& Data);
	U64 HashFunc(U64 Input, const String& Data);

	using WString = std::wstring;
	using StringView = std::string_view;

	String  WStringToString(const WString& wstr);
	WString StringToWString(const String& str);
#endif
}

namespace std
{
	template<>
	class hash<VulkanTest::String>
	{
	public:
		size_t operator()(const VulkanTest::String& str) const
		{
			return VulkanTest::CRC32(str.data());
		}
	};
};