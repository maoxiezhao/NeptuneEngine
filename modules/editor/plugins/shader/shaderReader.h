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
		struct PermutationsReader final : ITokenReader
		{
			ShaderFunctionReader* parent;
			I32 paramsCount = 0;

			const char* PERMUTATION_TOKENS[4] = {
				"META_PERMUTATION_1",
				"META_PERMUTATION_2",
				"META_PERMUTATION_3",
				"META_PERMUTATION_4"
			};

			PermutationsReader(ShaderFunctionReader* parent_) : parent(parent_) {}

			bool CheckToken(const TextReader::Token& token)override
			{
				for (I32 i = 0; i < ARRAYSIZE(PERMUTATION_TOKENS); i++)
				{
					if (token == PERMUTATION_TOKENS[i])
					{
						paramsCount = i + 1;
						return true;
					}
				}
				return false;
			}

			void Process(IShaderParser& shaderParser, TextReader& textReader)override
			{
				ASSERT(paramsCount > 0);
				ASSERT(parent != nullptr);

				auto& current = parent->current;
				I32 permutationIndex = (I32)current.permutations.size();
				auto& permutation = current.permutations.emplace_back();

				// Read permutation params
				// META_PERMUTIAION_2(k1 = v1, k2 = v2)
				TextReader::Token token;
				for (I32 i = 0; i < paramsCount; i++)
				{
					if (!textReader.CanRead())
					{
						Logger::Error("Invalid shader permutation params");
						return;
					}

					// Definition name
					textReader.ReadToken(token);
					if (token.length == 0)
					{
						Logger::Error("Invalid name of shader permutation param");
						return;
					}
						
					String name = token.ToString();

					// Check seperator is "="
					if (token.separator != TextReader::Separator('='))
					{
						if (token.separator.IsWhiteSpace())
							textReader.SkipWhiteSpaces();

						if (textReader.CurrentChar() != '=')
						{
							Logger::Error("Invalid shader permutation params");
							return;
						}
					}

					// Definition value
					textReader.ReadToken(token);
					if (token.length == 0)
					{
						Logger::Error("Invalid value of shader permutation param");
						return;
					}
					I32 value = 0;
					FromCString(Span(token.start, token.length), value);

					// Check seperator
					char checkChar = (i == paramsCount - 1) ? ')' : ',';
					if (token.separator != TextReader::Separator(checkChar))
					{
						Logger::Error("Invalid shader permutation params");
						return;
					}

					// Check is duplicated
					if (current.HasDefinition(permutationIndex, name))
					{
						Logger::Error("Already permutation defined %s.", name.c_str());
						return;
					}

					permutation.entries.push_back({ name, value });
				}
			}
		};

		ShaderFunctionReader()
		{
			ShaderMetaReader<MetaType>::readers.push_back(CJING_NEW(PermutationsReader)(this));
		}

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
