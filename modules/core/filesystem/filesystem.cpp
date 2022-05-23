#include "filesystem.h"
#include "core\platform\sync.h"
#include "core\platform\timer.h"

#include <queue>

namespace VulkanTest
{
	class DefaultFileSystemBackend;

	const AsyncLoadHandle AsyncLoadHandle::INVALID(0xffFFffFF);

	struct AsyncLoadJob
	{
		enum class State {
			Empty,
			FAILED,
			CANCELED,
		};
		State state = State::Empty;
		AsyncLoadCallback cb;
		MaxPathString path;
		U32 jobID = 0;
		OutputMemoryStream data;
	};

	// Async loading thread
	class AsyncLoadTask final : public Thread
	{
	public:
		AsyncLoadTask(DefaultFileSystemBackend& fs_) :
			fs(fs_)
		{}

		I32 Task() override;
		void Stop();

	private:
		DefaultFileSystemBackend& fs;
		bool isFinished = false;
	};

	////////////////////////////////////////////////////////////////////////////////
	//// FileSystemBackbend
	////////////////////////////////////////////////////////////////////////////////

	class DefaultFileSystemBackend : public FileSystemBackend
	{
	public:
		StaticString<MAX_PATH_LENGTH> basePath;
		U32 workerCount = 0;
		U32 lastJobID = 0;
		std::queue<AsyncLoadJob> pendingJobs;
		std::queue<AsyncLoadJob> finishedJobs;
		Mutex mutex;
		Semaphore semaphore;
		AsyncLoadTask* task;

	public:
		DefaultFileSystemBackend(const char* basePath) :
			semaphore(0, 0xFFff)
		{
			SetBasePath(basePath);

			task = CJING_NEW(AsyncLoadTask)(*this);
			task->Create("AsyncFileIO");
		}

		virtual ~DefaultFileSystemBackend()
		{
			task->Stop();
			task->Destroy();
			CJING_SAFE_DELETE(task);
		}

		void SetBasePath(const char* basePath_)override
		{
			Path::Normalize(basePath_, basePath.data);
			if (!EndsWith(basePath, Path::PATH_SLASH))
				basePath.append(Path::PATH_SLASH);
		}

		const char* GetBasePath()const
		{
			return basePath;
		}

		bool HasWork()const override
		{
			return false;
		}

		bool MoveFile(const char* from, const char* to)override
		{
			MaxPathString fullPathFrom(basePath, from);
			MaxPathString fullPathTo(basePath, to);
			return Platform::MoveFile(fullPathFrom, fullPathTo);
		}

		bool CopyFile(const char* from, const char* to)override
		{
			MaxPathString fullPathFrom(basePath, from);
			MaxPathString fullPathTo(basePath, to);
			return Platform::FileCopy(fullPathFrom, fullPathTo);
		}

		bool DeleteFile(const char* path)override
		{
			MaxPathString fullPath(basePath, path);
			return Platform::DeleteFile(fullPath);
		}
		
		bool FileExists(const char* path)override
		{
			MaxPathString fullPath(basePath, path);
			return Platform::FileExists(fullPath);
		}

		UniquePtr<File> OpenFile(const char* path, FileFlags flags)override
		{
			MaxPathString fullPath(basePath, path);
			MappedFile* file = CJING_NEW(MappedFile)(fullPath.c_str(), flags);
			//if (!file->IsValid())
			//	return nullptr;

			return UniquePtr<File>(file);
		}

		bool StatFile(const char* path, FileInfo& stat)override
		{
			MaxPathString fullPath(basePath, path);
			return Platform::StatFile(fullPath, stat);
		}

		U64 GetLastModTime(const char* path)override
		{
			MaxPathString fullPath(basePath, path);
			return Platform::GetLastModTime(fullPath);
		}

		std::vector<ListEntry> Enumerate(const char* path, int mask = (int)EnumrateMode::All)override
		{
			MaxPathString fullPath(basePath, path);
			std::vector<ListEntry> ret;
			auto* fileIt = Platform::CreateFileIterator(fullPath.c_str());
			if (fileIt == nullptr) {
				return ret;
			}

			ListEntry entry;
			while (Platform::GetNextFile(fileIt, entry)) {
				ret.push_back(entry);
			}
			Platform::DestroyFileIterator(fileIt);
			return ret;
		}

		bool LoadContext(const char* path, OutputMemoryStream& mem) override
		{
			auto file = OpenFile(path, FileFlags::DEFAULT_READ);
			if (file)
			{
				mem.Resize(file->Size());
				if (!file->Read(mem.Data(), mem.Size()))
				{
					Logger::Error("Failed to read %s", path);
					file->Close();
					return false;
				}

				file->Close();
				return true;
			}

			return false;
		}

		void ProcessAsync() override
		{
			// Process async loading jobs. Do callback for finished jobs

			Timer timer;
			while (true)
			{
				mutex.Lock();
				if (finishedJobs.empty())
				{
					mutex.Unlock();
					break;
				}

				AsyncLoadJob job = std::move(finishedJobs.front());
				finishedJobs.pop();

				ASSERT(workerCount > 0);
				workerCount--;
				mutex.Unlock();

				job.cb.Invoke(
					job.data.Size(), 
					(const U8*)job.data.Data(), 
					job.state != AsyncLoadJob::State::FAILED);

				// Cost too much time
				if (timer.GetTimeSinceStart() > 0.2f)
					break;
			}
		}

		AsyncLoadHandle LoadFileAsync(const Path& path, const AsyncLoadCallback& cb) override
		{
			if (path.IsEmpty())
				return AsyncLoadHandle::INVALID;

			ScopedMutex lock(mutex);
			workerCount++;
			lastJobID++;

			AsyncLoadJob job = {};
			job.jobID = lastJobID;
			job.path = path.c_str();
			job.cb = cb;
			pendingJobs.push(job);
			semaphore.Signal(1);
			return AsyncLoadHandle(lastJobID);
		}
	};

	// Async loading thread
	I32 AsyncLoadTask::Task()
	{
		while (!isFinished)
		{
			fs.semaphore.Wait();
			if (isFinished)
				break;

			// Get async loading job
			MaxPathString filePath;
			{
				ScopedMutex lock(fs.mutex);
				ASSERT(!fs.pendingJobs.empty());
				filePath = fs.pendingJobs.front().path;
			}

			// Do the job sync
			OutputMemoryStream mem;
			bool success = fs.LoadContext(filePath.c_str(), mem);

			// Push the current job into the finished queue
			{
				ScopedMutex lock(fs.mutex);
				ASSERT(!fs.pendingJobs.empty());
				AsyncLoadJob finishedJob = std::move(fs.pendingJobs.front());
				finishedJob.data = std::move(mem);
				if (success == false)
					finishedJob.state = AsyncLoadJob::State::FAILED;
			
				fs.finishedJobs.push(std::move(finishedJob));
				fs.pendingJobs.pop();
			}
		}

		return 0;
	}

	void AsyncLoadTask::Stop()
	{
		isFinished = true;
		fs.semaphore.Signal(1);
	}

	////////////////////////////////////////////////////////////////////////////////
	//// FileSystem
	////////////////////////////////////////////////////////////////////////////////

	FileSystem::~FileSystem()
	{
		backend.Reset();
	}

	UniquePtr<FileSystem> FileSystem::Create(const char* basePath)
	{
		UniquePtr<DefaultFileSystemBackend> backend = CJING_MAKE_UNIQUE<DefaultFileSystemBackend>(basePath);
		return UniquePtr<FileSystem>(CJING_NEW(FileSystem)(backend.Move()));
	}

	bool FileSystem::MoveFile(const char* from, const char* to)
	{
		return backend->MoveFile(from, to);
	}

	bool FileSystem::CopyFile(const char* from, const char* to)
	{
		return backend->CopyFile(from, to);
	}

	bool FileSystem::DeleteFile(const char* path)
	{
		return backend->DeleteFile(path);
	}

	bool FileSystem::FileExists(const char* path)
	{
		return backend->FileExists(path);
	}

	UniquePtr<File> FileSystem::OpenFile(const char* path, FileFlags flags)
	{
		return backend->OpenFile(path, flags);
	}

	bool FileSystem::StatFile(const char* path, FileInfo& stat)
	{
		return backend->StatFile(path, stat);
	}

	U64 FileSystem::GetLastModTime(const char* path)
	{
		return backend->GetLastModTime(path);
	}

	std::vector<ListEntry> FileSystem::Enumerate(const char* path, int mask)
	{
		return backend->Enumerate(path, mask);
	}

	void FileSystem::SetBasePath(const char* basePath_)
	{
		backend->SetBasePath(basePath_);
	}

	const char* FileSystem::GetBasePath() const
	{
		return backend->GetBasePath();
	}

	bool FileSystem::LoadContext(const char* path, OutputMemoryStream& mem)
	{
		return backend->LoadContext(path, mem);
	}

	bool FileSystem::HasWork()const
	{
		return backend->HasWork();
	}

	void FileSystem::ProcessAsync()
	{
		backend->ProcessAsync();
	}

	AsyncLoadHandle FileSystem::LoadFileAsync(const Path& path, const AsyncLoadCallback& cb)
	{
		return backend->LoadFileAsync(path, cb);
	}

	FileSystem::FileSystem(UniquePtr<FileSystemBackend>&& backend_) :
		backend(backend_.Move())
	{
	}
}