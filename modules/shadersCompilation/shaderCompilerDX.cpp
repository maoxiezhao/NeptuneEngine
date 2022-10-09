#include "shaderCompilerDX.h"
#include "core\utils\helper.h"

#ifdef CJING3D_PLATFORM_WIN32
#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>
#include <atlbase.h> // ComPtr

#define CiLoadLibrary(name) LoadLibrary(_T(name))
#define CiGetProcAddress(handle,name) GetProcAddress(handle, name)

#define VK_USE_PLATFORM_WIN32_KHR
#endif

// dxcompiler
#include "dxcompiler\inc\d3d12shader.h"
#include "dxcompiler\inc\dxcapi.h"

namespace VulkanTest
{
	struct CompilerImpl
	{
		DxcCreateInstanceProc DxcCreateInstance = nullptr;
		IDxcLibrary* library = nullptr;

		CompilerImpl()
		{
#ifdef _WIN32
#define LIBDXCOMPILER "dxcompiler.dll"
			HMODULE dxcompiler = CiLoadLibrary(LIBDXCOMPILER);
#elif defined(PLATFORM_LINUX)
#define LIBDXCOMPILER "libdxcompiler.so"
			HMODULE dxcompiler = CiLoadLibrary("./" LIBDXCOMPILER);
#endif
			if (dxcompiler != nullptr)
			{
				DxcCreateInstance = (DxcCreateInstanceProc)CiGetProcAddress(dxcompiler, "DxcCreateInstance");
				if (DxcCreateInstance != nullptr)
				{
					CComPtr<IDxcCompiler3> dxcCompiler;
					HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
					ASSERT(SUCCEEDED(hr));
					hr = DxcCreateInstance(CLSID_DxcLibrary, __uuidof(library), reinterpret_cast<void**>(&library));
					ASSERT(SUCCEEDED(hr));
					CComPtr<IDxcVersionInfo> info;
					hr = dxcCompiler->QueryInterface(&info);
					ASSERT(SUCCEEDED(hr));
					uint32_t minor = 0;
					uint32_t major = 0;
					hr = info->GetVersion(&major, &minor);
					ASSERT(SUCCEEDED(hr));
					Logger::Info("ShaderCompiler loaded (version:%u.$u)", std::to_string(major), std::to_string(minor));
				}
			}
			else
			{
				Logger::Warning("ShaderCompiler loaded failed");
			}
		}

		~CompilerImpl()
		{
			auto library_ = (IDxcLibrary*)library;
			if (library_)
				library_->Release();
		}
	};

	CompilerImpl& GetCompilerImpl()
	{
		static CompilerImpl impl;
		return impl;
	}

	struct IncludeHandler : public IDxcIncludeHandler
	{
		ShaderCompilationContext* ctx;
		IDxcLibrary* library;

		IncludeHandler(ShaderCompilationContext* ctx_, IDxcLibrary* library_)
		{
			ctx = ctx_;
			library = library_;
		}

		HRESULT STDMETHODCALLTYPE LoadSource(
			_In_z_ LPCWSTR pFilename,                                 // Candidate filename.
			_COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource  // Resultant source object for included file, nullptr if not found.
		) override
		{
			*ppIncludeSource = nullptr;
			const char* source;
			I32 sourceLength;
			std::string filename;
			Helper::StringConvert(pFilename, filename);
			if (!ShaderCompiler::GetIncludedFileSource(ctx, "", filename.c_str(), source, sourceLength))
				return E_FAIL;
			IDxcBlobEncoding* textBlob;
			if (FAILED(library->CreateBlobWithEncodingFromPinned((LPBYTE)source, sourceLength, CP_UTF8, &textBlob)))
				return E_FAIL;
			*ppIncludeSource = textBlob;
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(
			/* [in] */ REFIID riid,
			/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
		{
			if (riid == __uuidof(IDxcIncludeHandler) || riid == __uuidof(IUnknown))
			{
				AddRef();
				*ppvObject = this;
				return S_OK;
			}
			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return 1;
		}

		ULONG STDMETHODCALLTYPE Release() override
		{
			return 1;
		}
	};

	ShaderCompilerDX::ShaderCompilerDX() :
		ShaderCompiler(ShaderProfile::DirectX)
	{
		CompilerImpl& impl = GetCompilerImpl();
		if (impl.DxcCreateInstance == nullptr)
			Logger::Error("Failed to initialize shader compiler dx");
	}

	ShaderCompilerDX::~ShaderCompilerDX()
	{
	}

	bool ShaderCompilerDX::CompileShader(ShaderFunctionMeta& meta)
	{
		if (context->source->Empty())
			return false;

		CompilerImpl& impl = GetCompilerImpl();
		if (impl.DxcCreateInstance == nullptr)
			return false;

		// Write shader infos
		if (!WriteShaderInfo(meta))
			return false;

		// Prepare
		CComPtr<IDxcUtils> dxcUtils;
		CComPtr<IDxcCompiler3> dxcCompiler;
		HRESULT hr = impl.DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
		assert(SUCCEEDED(hr));
		hr = impl.DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
		assert(SUCCEEDED(hr));
		IDxcLibrary* library = (IDxcLibrary*)impl.library;

		auto options = context->options;
#if 0
		CComPtr<IDxcBlobEncoding> textBlob;
		if (FAILED(library->CreateBlobWithEncodingFromPinned((LPBYTE)options->source, options->sourceLength, CP_UTF8, &textBlob)))
			return true;

		Source.Ptr = textBlob->GetBufferPointer();
		Source.Size = textBlob->GetBufferSize();
		Source.Encoding = DXC_CP_ACP;
#else
		DxcBuffer Source;
		Source.Encoding = DXC_CP_ACP;
		Source.Ptr = (U8*)options->source;
		Source.Size = options->sourceLength;
#endif

		// https://github.com/microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll#dxcompiler-dll-interface
		std::vector<LPCWSTR> args;
		args.push_back(L"-D"); args.push_back(L"SPIRV");
		args.push_back(L"-spirv");
		args.push_back(L"-fspv-target-env=vulkan1.2");
		args.push_back(L"-fvk-use-dx-layout");
		args.push_back(L"-fvk-use-dx-position-w");
		args.push_back(L"-fvk-t-shift"); args.push_back(L"1000"); args.push_back(L"0");
		args.push_back(L"-fvk-u-shift"); args.push_back(L"2000"); args.push_back(L"0");
		args.push_back(L"-fvk-s-shift"); args.push_back(L"3000"); args.push_back(L"0");

		// shader model	
		args.push_back(L"-T");
		switch (meta.GetStage())
		{
		case GPU::ShaderStage::MS:
			args.push_back(L"ms_6_5");
			break;
		case GPU::ShaderStage::AS:
			args.push_back(L"as_6_5");
			break;
		case GPU::ShaderStage::VS:
			args.push_back(L"vs_6_0");
			break;
		case GPU::ShaderStage::HS:
			args.push_back(L"hs_6_0");
			break;
		case GPU::ShaderStage::DS:
			args.push_back(L"ds_6_0");
			break;
		case GPU::ShaderStage::GS:
			args.push_back(L"gs_6_0");
			break;
		case GPU::ShaderStage::PS:
			args.push_back(L"ps_6_0");
			break;
		case GPU::ShaderStage::CS:
			args.push_back(L"cs_6_0");
			break;
		case GPU::ShaderStage::LIB:
			args.push_back(L"lib_6_5");
			break;
		default:
			assert(0);
			return false;
		}

		// Entry point parameter:
		std::wstring wentry;
		std::string entrypoint = meta.name.c_str();
		Helper::StringConvert(entrypoint, wentry);
		args.push_back(L"-E");
		args.push_back(wentry.c_str());

		std::vector<std::wstring> definesString;
		GPU::ShaderVariantMap macros;
		for (U32 permutationIndex = 0; permutationIndex < (U32)meta.permutations.size(); permutationIndex++)
		{
			macros.clear();

			// Get macros from permutation
			meta.GetDefinitionsForPermutation(permutationIndex, macros);
			// Get global macros
			for (const auto& macro : context->options->Macros)
				macros.push_back(macro);
			
			// defines
			definesString.resize(macros.size()); // keep ptr
			for (int i = 0; i < macros.size(); i++)
			{
				const auto& macro = macros[i];
				if (macro.name.empty())
					continue;

				std::string def = macro.name + "=" + std::to_string(macro.definition);
				std::wstring& wstr = definesString[i];
				Helper::StringConvert(def, wstr);
				args.push_back(L"-D");
				args.push_back(wstr.c_str());
			}

			IDxcLibrary* library = (IDxcLibrary*)impl.library;
			IncludeHandler includeHandler(context, library);

			CComPtr<IDxcResult> pResults;
			hr = dxcCompiler->Compile(
				&Source,					// Source buffer.
				args.data(),                // Array of pointers to arguments.
				(UINT32)args.size(),		// Number of arguments.
				&includeHandler,		    // User-provided interface to handle #include directives (optional).
				IID_PPV_ARGS(&pResults)		// Compiler output status, buffer, and errors.
			);
			assert(SUCCEEDED(hr));

			// Print errors if present.
			CComPtr<IDxcBlobUtf8> pErrors = nullptr;
			hr = pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
			assert(SUCCEEDED(hr));
			if (pErrors != nullptr && pErrors->GetStringLength() != 0)
				Logger::Error(pErrors->GetStringPointer());

			// Quit if the compilation failed.
			HRESULT hrStatus;
			hr = pResults->GetStatus(&hrStatus);
			if (FAILED(hrStatus))
				return false;

			// Get shader binary
			CComPtr<IDxcBlob> shaderBuffer = nullptr;
			hr = pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBuffer), nullptr);
			if (FAILED(hr) || shaderBuffer == nullptr)
			{
				Logger::Error("IDxcOperationResult::GetResult failed.");
				return false;
			}

			// Reflect shader to get resource layout
			GPU::ShaderResourceLayout resLayout;
			if (!GPU::Shader::ReflectShader(resLayout, (const U32*)shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize()))
			{
				Logger::Error("Failed to reflect shader %s", meta.name);
				return false;
			}

			// Write shader
			if (!WriteShaderFunctionPermutation(meta, permutationIndex, resLayout, shaderBuffer->GetBufferPointer(), (I32)shaderBuffer->GetBufferSize()))
				return false;
		}

		return true;
	}
}