#pragma once

#include "editorPlugin.h"
#include "shaderMeta.h"
#include "core\utils\textReader.h"

namespace VulkanTest
{
namespace Editor
{
	struct IShaderParser
	{
		virtual TextReader& GetTextReader() = 0;
	};

	struct ITokenReader
	{
		virtual bool CheckToken(const TextReader::Token& token) = 0;
		virtual void Process(IShaderParser& shaderParser, TextReader& textReader) = 0;
	};

	template<typename ReaderType>
	struct ITokenReadersContainer
	{
		Array<ReaderType*> readers;

		~ITokenReadersContainer()
		{
			for (auto reader : readers)
				CJING_SAFE_DELETE(reader);
			readers.clear();
		}

		virtual bool ProcessChildren(const TextReader::Token& token, IShaderParser& shaderParser)
		{
			for (auto reader : readers)
			{
				if (reader->CheckToken(token))
				{
					reader->Process(shaderParser, shaderParser.GetTextReader());
					return true;
				}
			}
			return false;
		}
	};

	struct ShaderMetaCollector : ITokenReader
	{
		virtual void CollectResults(IShaderParser& parser, ShaderMeta* result) = 0;
	};

	template<typename MetaType>
	struct ShaderMetaReader : ShaderMetaCollector, ITokenReadersContainer<ITokenReader>
	{
		Array<MetaType> metaCache;
		MetaType current;

		void Process(IShaderParser& shaderParser, TextReader& textReader) override
		{
			OnParseBefore(shaderParser, textReader);
			if (!OnParse(shaderParser, textReader))
				return;
			OnParseAfter(shaderParser, textReader);
		}

		virtual bool OnParse(IShaderParser& parser, TextReader& textReader)
		{
			TextReader::Token token;
			while (textReader.CanRead())
			{
				textReader.ReadToken(token);
				if (!ProcessChildren(token, parser))
					break;
			}

			textReader.ReadToken(token);
			current.name = token.ToString();
			return true;
		}

		void CollectResults(IShaderParser& parser, ShaderMeta* result)override
		{
			// Check shader function names
			std::unordered_map<U64, bool> set;
			for (const auto& cache : metaCache)
			{
				auto hash = StringID(cache.name.c_str()).GetHashValue();
				auto it = set.find(hash);
				if (it != set.end())
				{
					Logger::Error("Duplicated shader function names.");
					return;
				}
				set[hash] = true;
			}

			FlushCache(parser, result);
		}

		virtual void OnParseBefore(IShaderParser& parser, TextReader& textReader) = 0;
		virtual void OnParseAfter(IShaderParser& parser, TextReader& textReader) = 0;
		virtual void FlushCache(IShaderParser& parser, ShaderMeta* result) = 0;
	};

	template<typename MetaType>
	struct ShaderFunctionReader : ShaderMetaReader<MetaType>
	{
		void OnParseBefore(IShaderParser& parser, TextReader& textReader) override
		{
			auto& current = ShaderMetaReader<MetaType>::current;
			current.name.clear();
			current.permutations.clear();
			current.visible = true;

			// Read shader meta info
			// ex: META_VS(true)
			// ex: META_PS(true)
			TextReader::Token token;
			textReader.ReadToken(token);
			if (token == "true" || token == "1")
			{
				// Do nothing
			}
			else if (token == "false" || token == "0")
			{
				current.visible = false;
			}

			// Skip current line
			textReader.ReadLine();
		}

		void OnParseAfter(IShaderParser& parser, TextReader& textReader) override
		{
			auto& current = ShaderMetaReader<MetaType>::current;
			if (current.permutations.empty())
			{
				// Just add blank permutation (rest of the code will work without any hacks for empty permutations list)
				current.permutations.push_back(ShaderPermutation());
			}

			if (current.visible)
			{
				ShaderMetaReader<MetaType>::metaCache.push_back(current);
			}
		}
	};

	struct StripLineReader final : ITokenReader
	{
		TextReader::Token checkToken;

		explicit StripLineReader(const char* token) : checkToken(token){}

		bool CheckToken(const TextReader::Token& token)override
		{
			return checkToken == token;
		}
		
		void Process(IShaderParser& shaderParser, TextReader& textReader)override
		{
			textReader.ReadLine();
		}
	};

#define DECLARE_SHADER_META_READER(tokenName, collection) public: \
	bool CheckToken(const TextReader::Token& token) override \
	{ return token == tokenName; } \
	void FlushCache(IShaderParser& parser, ShaderMeta* result) override \
	{ for (const auto& cache : metaCache) result->collection.push_back(cache); }

	struct VertexShaderFunctionReader : ShaderFunctionReader<VertexShaderMeta>
	{
		DECLARE_SHADER_META_READER("META_VS", vs);
	};

	struct PixelShaderFunctionReader : ShaderFunctionReader<PixelShaderMeta>
	{
		DECLARE_SHADER_META_READER("META_PS", ps);
	};

	struct ComputeShaderFunctionReader : ShaderFunctionReader<ComputeShaderMeta>
	{
		DECLARE_SHADER_META_READER("META_CS", cs);

		ComputeShaderFunctionReader()
		{
			readers.push_back(CJING_NEW(StripLineReader)("numthreads"));
		}
	};
}
}
