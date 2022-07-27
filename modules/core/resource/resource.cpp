#include "resource.h"
#include "resourceManager.h"
#include "core\utils\string.h"
#include "core\compress\compressor.h"

namespace VulkanTest
{
	const ResourceType ResourceType::INVALID_TYPE("");

	ResourceType::ResourceType(const char* typeName)
	{
		ASSERT(typeName[0] == 0 || (typeName[0] >= 'a' && typeName[0] <= 'z') || (typeName[0] >= 'A' && typeName[0] <= 'Z'));
		type = StringID(typeName);
	}

	void ResourceDeleter::operator()(Resource* res)
	{
		if (res->resFactory.IsUnloadEnable())
			res->DoUnload();
	}

	Resource::~Resource() = default;

	//void Resource::IncRefCount()
	//{
	//	refCount++;
	//}

	//void Resource::DecRefCount()
	//{
	//	ASSERT(refCount > 0);
	//	if (--refCount == 0 && resFactory.IsUnloadEnable())
	//		DoUnload();
	//}

	void Resource::Refresh()
	{
		if (currentState == State::EMPTY)
			return;

		State oldState = currentState;
		currentState = State::EMPTY;
		cb.Invoke(oldState, currentState, *this);
		CheckState();
	}

	void Resource::AddDependency(Resource& depRes)
	{
		ASSERT(desiredState != State::EMPTY);

		depRes.cb.Bind<&Resource::OnStateChanged>(this);
		if (depRes.IsEmpty())
			emptyDepCount++;
		if (depRes.IsFailure())
			failedDepCount++;

		CheckState();
	}

	void Resource::RemoveDependency(Resource& depRes)
	{
		depRes.cb.Unbind<&Resource::OnStateChanged>(this);
		if (depRes.IsEmpty())
		{
			ASSERT(emptyDepCount > 1 || (emptyDepCount == 1 && !asyncHandle.IsValid()));
			emptyDepCount--;
		}
		if (depRes.IsFailure())
		{
			ASSERT(failedDepCount > 0);
			failedDepCount--;
		}

		CheckState();
	}

	ResourceManager& Resource::GetResourceManager()
	{
		return resFactory.GetResourceManager();
	}

	Resource::Resource(const Path& path_, ResourceFactory& resFactory_) :
		emptyDepCount(1),
		failedDepCount(0),
		currentState(State::EMPTY),
		desiredState(State::EMPTY),
		resSize(0),
		path(path_),
		resFactory(resFactory_),
		asyncHandle(AsyncLoadHandle::INVALID)
	{
	}

	void Resource::DoLoad()
	{
		// Check resource is loaded
		if (desiredState == State::READY)
			return;
		desiredState = State::READY;

		FileSystem* fileSystem = resFactory.GetResourceManager().GetFileSystem();
#if DEBUG
		if (!NeedExport())
		{
			OutputMemoryStream mem;
			bool ret = fileSystem->LoadContext(path.c_str(), mem);
			OnFileLoaded(mem.Size(), (const U8*)mem.Data(), ret);
			return;
		}
#endif
		if (asyncHandle.IsValid())
			return;

		AsyncLoadCallback cb;
		cb.Bind<&Resource::OnFileLoaded>(this);

		const U64 pathHash = path.GetHashValue();
		StaticString<MAX_PATH_LENGTH> fullResPath(".export/resources/", pathHash, ".res");
		asyncHandle = fileSystem->LoadFileAsync(Path(fullResPath), cb);
	}

	void Resource::DoUnload()
	{
		if (asyncHandle.IsValid())
		{
			// Cancel when res is loading
			FileSystem* fileSystem = resFactory.GetResourceManager().GetFileSystem();
			// TODO
			// fileSystem.Cancel(asyncHandle);
			asyncHandle = AsyncLoadHandle::INVALID;
		}

		desiredState = State::EMPTY;
		OnUnLoaded();
		ASSERT(emptyDepCount <= 1);

		// Refresh current state
		resSize = 0;
		emptyDepCount = 1;
		failedDepCount = 0;
		CheckState();
	}

	void Resource::CheckState()
	{
		// Refresh the current state according to failedDep and emtpyDep
		State oldState = currentState;
		if (currentState != State::FAILURE && failedDepCount > 0)
		{
			currentState = State::FAILURE;
			cb.Invoke(oldState, currentState, *this);
		}

		if (failedDepCount == 0)
		{
			if (currentState != State::READY && desiredState != State::EMPTY && emptyDepCount == 0)
			{
				currentState = State::READY;
				cb.Invoke(oldState, currentState, *this);
			}

			if (currentState != State::EMPTY && emptyDepCount > 0)
			{
				currentState = State::EMPTY;
				cb.Invoke(oldState, currentState,*this);
			}
		}
	}

	void Resource::OnCreated(State state)
	{
		ASSERT(emptyDepCount == 1);
		ASSERT(failedDepCount == 0);

		currentState = state;
		desiredState = State::READY;
		failedDepCount = state == State::FAILURE ? 1 : 0;
		emptyDepCount = 0;
	}

#if DEBUG
	bool Resource::NeedExport()const
	{
		return true;
	}
#endif

	void Resource::OnFileLoaded(U64 size, const U8* mem, bool success)
	{
		ASSERT(asyncHandle.IsValid());
		asyncHandle = AsyncLoadHandle::INVALID;

		// Desired state changed when file loading, so is a invalid loading
		if (desiredState != State::READY)
			return;

		ASSERT(currentState != State::READY);
		ASSERT(emptyDepCount == 1);

		if (success == false)
		{
			Logger::Error("Failed to load %s", GetPath().c_str());
			emptyDepCount--;
			failedDepCount++;
			CheckState();
			return;
		}

		// Ok, file loaded success, let's process the file as CompiledResource
		// CompiledReource:
		// ---------------------------
		// |  CompiledResourceHeader |
		// ---------------------------
		// |   Bytes context         |
		// ---------------------------

		const CompiledResourceHeader* resHeader = (const CompiledResourceHeader*)mem;
		if (size < sizeof(CompiledResourceHeader))
		{
			Logger::Error("Invalid compiled resource %s", GetPath().c_str());
			failedDepCount++;			
			goto CHECK_STATE;
		}
		
		if (resHeader->magic != CompiledResourceHeader::MAGIC)
		{
			Logger::Error("Invalid compiled resource %s", GetPath().c_str());
			failedDepCount++;
			goto CHECK_STATE;
		}

		if (resHeader->version != CompiledResourceHeader::VERSION)
		{
			Logger::Error("Invalid compiled resource %s", GetPath().c_str());
			failedDepCount++;
			goto CHECK_STATE;
		}

		if (resHeader->isCompressed)
		{
			OutputMemoryStream tmp;
			tmp.Resize(resHeader->originSize);
			I32 decompressedSize = Compressor::Decompress(
				(const char*)mem + sizeof(CompiledResourceHeader),
				(char*)tmp.Data(),
				I32(size - sizeof(CompiledResourceHeader)),
				tmp.Size());

			if (decompressedSize != resHeader->originSize)
				failedDepCount++;
			else if (!OnLoaded(decompressedSize, tmp.Data()))
				failedDepCount++;

			resSize = resHeader->originSize;
		}
		else
		{
			bool ret = OnLoaded(size - sizeof(*resHeader), mem + sizeof(*resHeader));
			if (ret == false)
			{
				Logger::Error("Failed to load resource %s", GetPath().c_str());
				failedDepCount++;
			}	
			resSize = resHeader->originSize;
		}

	CHECK_STATE:
		ASSERT(emptyDepCount > 0);
		emptyDepCount--;
		CheckState();
		asyncHandle = AsyncLoadHandle::INVALID;		
	}

	void Resource::OnStateChanged(State oldState, State newState, Resource& res)
	{
		ASSERT(oldState != newState);
		ASSERT(currentState != State::EMPTY || desiredState != State::EMPTY);

		if (oldState == State::EMPTY)
		{
			ASSERT(emptyDepCount > 0);
			emptyDepCount--;
		}
		if (oldState == State::FAILURE)
		{
			ASSERT(failedDepCount > 0);
			failedDepCount--;
		}

		if (newState == State::EMPTY)
			emptyDepCount++;
		if (newState == State::FAILURE)
			failedDepCount++;

		CheckState();
	}
}