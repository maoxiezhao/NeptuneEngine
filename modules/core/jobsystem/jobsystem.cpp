#include "jobsystem.h"
#include "core\memory\memory.h"
#include "core\platform\fiber.h"
#include "core\platform\platform.h"
#include "core\platform\sync.h"
#include "core\platform\atomic.h"

#pragma warning( push )
#pragma warning (disable : 6385)

namespace VulkanTest
{
namespace Jobsystem
{
    //////////////////////////////////////////////////////////////
    // Definition
    const U32 MAX_FIBER_COUNT = 512;
    const U32 MAX_JOB_HANDLE_COUNT = 4096;
    const U32 HANDLE_ID_MASK = 0x0000ffff;
    const U32 HANDLE_GENERATION_MASK = 0xffff0000;

#ifdef _WIN32
    static void __stdcall FiberFunc(void* data);
#else
    static void FiberFunc(void* data);
#endif

    struct ManagerImpl;
    struct WorkerThread;

    struct JobImpl
    {
        JobFunc task = nullptr;
        void* data = nullptr;
        JobHandle finishHandle;
        JobHandle precondition;
        U8 workerIndex;
    };

    struct JobCounter
    {
        volatile I32 value = 0;
        JobImpl nextJob;
        JobHandle sibling = INVALID_HANDLE;
        U32 generation = 0;	// use for check
    };

    struct WorkerFiber
    {
        U32 index = 0;
        Fiber::Handle handle = Fiber::INVALID_HANDLE;
        JobImpl currentJob;
    };

    struct ManagerImpl
    {
    public:
        Mutex sync;
        ConditionMutex jobQueueLock;
        JobCounter handleCounters[MAX_JOB_HANDLE_COUNT];
        std::vector<U32> handlePool;
        std::vector<WorkerFiber*> readyFibers;
        std::vector<WorkerFiber*> freeFibers;
        WorkerFiber fiberPool[MAX_FIBER_COUNT];
        std::vector<WorkerThread*> workers;
        std::vector<JobImpl> jobQueue;

    public:
        ManagerImpl();

        JobHandle AllocateHandle();
        void IncHandle(JobHandle* jobHandle);
        void DecHandle(JobHandle jobHandle);
        bool IsHandleValid(JobHandle jobHandle)const;
        bool IsHandleZero(JobHandle jobHandle, bool isLock);
        void ReleaseHandle(JobHandle jobHandle, bool isLock);
        void PushJob(JobImpl job);
    };

    static LocalPtr<ManagerImpl> gManager;
    static thread_local WorkerThread* gWorker = nullptr;

    WorkerThread* GetWorker()
    {
        return gWorker;
    }

    struct WorkerThread : Thread
    {
    public:
        U32 workderIndex;
        ManagerImpl& manager;
        bool isFinished = false;
        Fiber::Handle primaryFiber = Fiber::INVALID_HANDLE;
        WorkerFiber* currentFiber = nullptr;
        std::vector<WorkerFiber*> readyFibers;
        std::vector<JobImpl> jobQueue;

#ifdef JOB_SYSTEM_DEBUG
        volatile I32 numFinishedJob = 0;
#endif // JOB_SYSTEM_DEBUG

    public:
        WorkerThread(ManagerImpl& manager_, U32 workerIndex_) :
            manager(manager_),
            workderIndex(workerIndex_)
        {
        }

        ~WorkerThread()
        {
        }

        int Task() override
        {
            Platform::SetCurrentThreadIndex(workderIndex + 1);

            gWorker = this;
            primaryFiber = Fiber::Create(Fiber::THIS_THREAD);
            gManager->sync.Lock();

            WorkerFiber* fiber = gManager->freeFibers.back();
            gManager->freeFibers.pop_back();
            if (!Fiber::IsValid(fiber->handle))
                fiber->handle = Fiber::Create(64 * 1024, FiberFunc, fiber);

            gWorker->currentFiber = fiber;
            Fiber::SwitchTo(gWorker->primaryFiber, fiber->handle);
            return 0;
        }
    };

    ManagerImpl::ManagerImpl()
    {
        handlePool.resize(MAX_JOB_HANDLE_COUNT);
        for (int i = 0; i < MAX_JOB_HANDLE_COUNT; i++)
        {
            handleCounters[i].sibling = INVALID_HANDLE;
            handlePool[i] = i;;
        }
    }

    JobHandle ManagerImpl::AllocateHandle()
    {
        if (handlePool.empty())
            return INVALID_HANDLE;

        U32 handle = handlePool.back();
        handlePool.pop_back();
        JobCounter& counter = handleCounters[handle & HANDLE_ID_MASK];
        counter.value = 1;
        counter.sibling = INVALID_HANDLE;
        counter.nextJob.task = nullptr;

        return (handle & HANDLE_ID_MASK) | counter.generation;
    }

    void ManagerImpl::IncHandle(JobHandle* jobHandle)
    {
        ASSERT(jobHandle != nullptr);
        ScopedMutex lock(sync);
        if (IsHandleValid(*jobHandle) && !IsHandleZero(*jobHandle, false))
            handleCounters[*jobHandle & HANDLE_ID_MASK].value++;
        else
            *jobHandle = AllocateHandle();
    }

    void ManagerImpl::DecHandle(JobHandle jobHandle)
    {
        ScopedMutex lock(sync);
        JobCounter& counter = handleCounters[jobHandle & HANDLE_ID_MASK];
        counter.value--;
        if (counter.value > 0)
            return;

        ReleaseHandle(jobHandle, false);
    }

    bool ManagerImpl::IsHandleValid(JobHandle jobHandle)const
    {
        return jobHandle != INVALID_HANDLE;
    }

    bool ManagerImpl::IsHandleZero(JobHandle jobHandle, bool isLock)
    {
        if (!IsHandleValid(jobHandle))
            return false;

        const U32 id = jobHandle & HANDLE_ID_MASK;
        const U32 generation = jobHandle & HANDLE_GENERATION_MASK;
        if (isLock) sync.Lock();
        bool ret = handleCounters[id].value == 0 || handleCounters[id].generation != generation;
        if (isLock) sync.Unlock();
        return ret;
    }

    void ManagerImpl::ReleaseHandle(JobHandle jobHandle, bool isLock)
    {
        if (isLock) sync.Lock();
        JobHandle iter = jobHandle;
        while (IsHandleValid(iter))
        {
            JobCounter& counter = handleCounters[iter & HANDLE_ID_MASK];
            if (counter.nextJob.task != nullptr)
            {
                ScopedConditionMutex lock(jobQueueLock);
                PushJob(counter.nextJob);
            }

            counter.generation = (((counter.generation >> 16) + 1) & 0xffff) << 16;
            handlePool.push_back(iter & HANDLE_ID_MASK | counter.generation);
            counter.nextJob.task = nullptr;
            iter = counter.sibling;
        }
        if (isLock) sync.Unlock();
    }

    void ManagerImpl::PushJob(JobImpl job)
    {
        if (job.workerIndex == ANY_WORKER)
        {
            jobQueue.push_back(job);
            for (auto worker : workers)
                worker->Wakeup();
        }
        else
        {
            WorkerThread* worker = gManager->workers[job.workerIndex % gManager->workers.size()];
            worker->jobQueue.push_back(job);
            worker->Wakeup();
        }
    }

    //////////////////////////////////////////////////////////////
    // Methods

    bool Initialize(U32 numWorkers)
    {
        Platform::SetCurrentThreadIndex(0);

        gManager.Create();

        for (int i = 0; i < MAX_FIBER_COUNT; i++)
        {
            WorkerFiber* fiber = &gManager->fiberPool[i];
            fiber->index = i;
            fiber->handle = Fiber::INVALID_HANDLE;
            gManager->freeFibers.push_back(fiber);
        }

        numWorkers = std::min(64u, numWorkers);
        gManager->workers.reserve(numWorkers);
        for (U32 i = 0; i < numWorkers; i++)
        {
            WorkerThread* worker = CJING_NEW(WorkerThread)(*gManager, i);
            if (worker->Create("Worker"))
            {
                gManager->workers.push_back(worker);
                worker->SetAffinity((U64)1u << i);
            }
        }

        return !gManager->workers.empty();
    }

    void Uninitialize()
    {
        // Clear workers
        for (auto worker : gManager->workers)
        {
            worker->isFinished = true;
            worker->Wakeup();
        }
        for (auto worker : gManager->workers)
        {
            while (!worker->IsFinished())
                worker->Wakeup();
            worker->Destroy();
            CJING_SAFE_DELETE(worker);
        }
        gManager->workers.clear();

        // Clear fibers
        for (auto& fiber : gManager->fiberPool)
        {
            if (Fiber::IsValid(fiber.handle))
                Fiber::Destroy(fiber.handle);
        }

        gManager.Destroy();
    }

    void RunInternal(JobFunc task, void* data, bool doLock, JobHandle precondition, JobHandle* handle, int workerIndex)
    {
        JobImpl job = {};
        job.data = data;
        job.task = task;
        job.precondition = precondition;
        job.workerIndex = U8(workerIndex != ANY_WORKER ? workerIndex % gManager->workers.size() : ANY_WORKER);

        if (doLock) gManager->sync.Lock();
        job.finishHandle = [&]() {
            if (handle == nullptr)
            {
                return INVALID_HANDLE;
            }

            if (gManager->IsHandleValid(*handle) && !gManager->IsHandleZero(*handle, false))
            {
                gManager->handleCounters[*handle & HANDLE_ID_MASK].value++;
                return *handle;
            }

            return gManager->AllocateHandle();
        }();

        if (handle != nullptr)
            *handle = job.finishHandle;

        if (!gManager->IsHandleValid(precondition) || gManager->IsHandleZero(precondition, false))
        {
            ScopedConditionMutex lock(gManager->jobQueueLock);
            gManager->PushJob(job);
        }
        else
        {
            auto& preCounter = gManager->handleCounters[precondition & HANDLE_ID_MASK];
            if (preCounter.nextJob.task != nullptr)
            {
                auto slibingHandle = gManager->AllocateHandle();
                auto& slibingCounter = gManager->handleCounters[slibingHandle & HANDLE_ID_MASK];
                slibingCounter.nextJob = job;
                slibingCounter.sibling = preCounter.sibling;
                preCounter.sibling = slibingHandle;

            }
            else
            {
                preCounter.nextJob = job;
            }
        }

        if (doLock) gManager->sync.Unlock();
    }

    void Run(const JobInfo& jobInfo, JobHandle* handle)
    {
        RunInternal(
            jobInfo.jobFunc,
            jobInfo.data,
            true,
            jobInfo.precondition,
            handle,
            ANY_WORKER
        );
    }

    void Run(void*data, JobFunc func, JobHandle* handle)
    {
        RunInternal(
            func,
            data,
            true,
            INVALID_HANDLE,
            handle,
            ANY_WORKER
        );
    }

    void RunEx(void* data, JobFunc func, JobHandle* handle, JobHandle precondition, int workerIndex)
    {
        RunInternal(
            func,
            data,
            true,
            precondition,
            handle,
            workerIndex
        );
    }

    void Wait(JobHandle handle)
    {
        gManager->sync.Lock();
        if (gManager->IsHandleZero(handle, false))
        {
            gManager->sync.Unlock();
            return;
        }
    
        if (GetWorker() == nullptr)
        {
            while (!gManager->IsHandleZero(handle, false))
            {
                gManager->sync.Unlock();
                Platform::Sleep(1);
                gManager->sync.Lock();
            }

            gManager->sync.Unlock();
        }
        else
        {
            WorkerFiber* currentFiber = GetWorker()->currentFiber;
            RunInternal([](void* data) {
                ScopedConditionMutex lock(gManager->jobQueueLock);
                WorkerFiber* fiber = (WorkerFiber*)data;
                if (fiber->currentJob.workerIndex == ANY_WORKER)
                {
                    gManager->readyFibers.push_back(fiber);
                    for (auto worker : gManager->workers)
                        worker->Wakeup();
                }
                else
                {
                    WorkerThread* worker = gManager->workers[fiber->currentJob.workerIndex % gManager->workers.size()];
                    worker->readyFibers.push_back(fiber);
                    worker->Wakeup();
                }

                }, currentFiber, false, handle, nullptr, 0); // currentFiber->currentJob.workerIndex);
            
            WorkerFiber* newFiber = gManager->freeFibers.back();
            gManager->freeFibers.pop_back();
            if (!Fiber::IsValid(newFiber->handle))
                newFiber->handle = Fiber::Create(64 * 1024, FiberFunc, newFiber);

            GetWorker()->currentFiber = newFiber;
            Fiber::SwitchTo(currentFiber->handle, newFiber->handle);
            GetWorker()->currentFiber = currentFiber;
            gManager->sync.Unlock();
        }
    }

#ifdef _WIN32
    static void __stdcall FiberFunc(void* data)
#else
    static void FiberFunc(void* data);
#endif
    {
        gManager->sync.Unlock();

        WorkerFiber* currentFiber = (WorkerFiber*)(data);
        WorkerThread* worker = GetWorker();
        while (!worker->isFinished)
        {
            WorkerFiber* readyFiber = nullptr;
            JobImpl job;
            while (!worker->isFinished)
            {
                ScopedConditionMutex lock(gManager->jobQueueLock);

                // Local
                if (!worker->readyFibers.empty())
                {
                    readyFiber = worker->readyFibers.back();
                    worker->readyFibers.pop_back();
                    break;
                }
                if (!worker->jobQueue.empty())
                {
                    job = worker->jobQueue.back();
                    worker->jobQueue.pop_back();
                    break;
                }
                
                // Global
                if (!gManager->readyFibers.empty())
                {
                    readyFiber = gManager->readyFibers.back();
                    gManager->readyFibers.pop_back();
                    break;
                }
                if (!gManager->jobQueue.empty())
                {
                    job = gManager->jobQueue.back();
                    gManager->jobQueue.pop_back();
                    break;
                }

                worker->Sleep(gManager->jobQueueLock);
            }

            if (worker->isFinished)
                break;

            if (readyFiber != nullptr)
            {
                worker->currentFiber = readyFiber;

                gManager->sync.Lock();
                gManager->freeFibers.push_back(currentFiber);
                Fiber::SwitchTo(currentFiber->handle, readyFiber->handle);
                gManager->sync.Unlock();

                worker = GetWorker();
                worker->currentFiber = currentFiber;
            }
            else if (job.task != nullptr)
            {
#ifdef JOB_SYSTEM_DEBUG
                AtomicIncrement(&worker->numFinishedJob);
#endif
                currentFiber->currentJob = job;
                job.task(job.data);
                currentFiber->currentJob.task = nullptr;

                if (gManager->IsHandleValid(job.finishHandle))
                    gManager->DecHandle(job.finishHandle);

                worker = GetWorker();
            }
        }

        Fiber::SwitchTo(currentFiber->handle, worker->primaryFiber);
    }

#ifdef JOB_SYSTEM_DEBUG
    void ShowDebugInfo()
    {
        std::cout << "Worker infos:" << std::endl;
        for (auto& worker : gManager->workers)
        {
            std::cout << worker->workderIndex << "; numFinishedJob:";
            std::cout << worker->numFinishedJob;
            std::cout << std::endl;
        }
    }
#endif
}
}

#pragma warning (pop)