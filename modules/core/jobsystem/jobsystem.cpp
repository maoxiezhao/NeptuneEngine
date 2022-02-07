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
        U8 workerIndex;
    };

    struct WorkerFiber
    {
        U32 index = 0;
        Fiber::Handle handle = Fiber::INVALID_HANDLE;
        JobImpl currentJob;
    };

    struct JobWaitor
    {
        JobWaitor* next;
        WorkerFiber* fiber;
    };

    struct JobCounter
    {
        volatile I32 value = 0;
        JobWaitor* waitor = nullptr;
        U32 generation = 0;	// use for check
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
        bool IsHandleValid(JobHandle jobHandle)const;
        bool IsHandleZero(JobHandle jobHandle);
        bool Trigger(JobHandle jobHandle);
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
            handlePool[i] = i;
    }

    JobHandle ManagerImpl::AllocateHandle()
    {
        if (handlePool.empty())
            return INVALID_HANDLE;

        U32 handle = handlePool.back();
        handlePool.pop_back();
        JobCounter& counter = handleCounters[handle & HANDLE_ID_MASK];
        counter.value = 1;
        counter.waitor = nullptr;
        return (handle & HANDLE_ID_MASK) | counter.generation;
    }

    bool ManagerImpl::IsHandleValid(JobHandle jobHandle)const
    {
        return jobHandle != INVALID_HANDLE;
    }

    bool ManagerImpl::IsHandleZero(JobHandle jobHandle)
    {
        if (!IsHandleValid(jobHandle))
            return false;

        const U32 id = jobHandle & HANDLE_ID_MASK;
        const U32 generation = jobHandle & HANDLE_GENERATION_MASK;
        return handleCounters[id].value == 0 || handleCounters[id].generation != generation;
    }

    bool ManagerImpl::Trigger(JobHandle jobHandle)
    {
        ScopedMutex lock(sync);
        JobCounter& counter = handleCounters[jobHandle & HANDLE_ID_MASK];
        counter.value--;
        if (counter.value > 0)
            return false;

        JobWaitor* waitor = counter.waitor;
        while (waitor != nullptr)
        {
            JobWaitor* next = waitor->next;
            U8 workerIndex = waitor->fiber->currentJob.workerIndex;
            ScopedConditionMutex lock(jobQueueLock);
            if (workerIndex == ANY_WORKER)
            {
                readyFibers.push_back(waitor->fiber);
                for (auto worker : workers)
                    worker->Wakeup();
            }
            else
            {
                WorkerThread* worker = gManager->workers[workerIndex % gManager->workers.size()];
                worker->readyFibers.push_back(waitor->fiber);
                worker->Wakeup();
            }
            waitor = next;
        }
        counter.waitor = nullptr;
        return true;
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

    void RunInternal(JobFunc task, void* data, JobHandle* handle, int workerIndex)
    {
        JobImpl job = {};
        job.data = data;
        job.task = task;
        job.workerIndex = U8(workerIndex != ANY_WORKER ? workerIndex % gManager->workers.size() : ANY_WORKER);

        ScopedMutex guard(gManager->sync);
        job.finishHandle = [&]() 
        {
            if (handle == nullptr)
                return INVALID_HANDLE;

            if (gManager->IsHandleValid(*handle) && !gManager->IsHandleZero(*handle))
            {
                gManager->handleCounters[*handle & HANDLE_ID_MASK].value++;
                return *handle;
            }

            return gManager->AllocateHandle();
        }();

        if (handle != nullptr)
            *handle = job.finishHandle;

        ScopedConditionMutex lock(gManager->jobQueueLock);
        gManager->PushJob(job);
    }

    void Run(void*data, JobFunc func, JobHandle* handle, U8 workerIndex)
    {
        RunInternal(
            func,
            data,
            handle,
            workerIndex
        );
    }

    void Wait(JobHandle handle)
    {
        gManager->sync.Lock();
        if (gManager->IsHandleZero(handle))
        {
            gManager->sync.Unlock();
            return;
        }
    
        if (GetWorker() == nullptr)
        {
            while (!gManager->IsHandleZero(handle))
            {
                gManager->sync.Unlock();
                Platform::Sleep(1);
                gManager->sync.Lock();
            }

            gManager->sync.Unlock();
            return;
        }

        JobCounter& counter = gManager->handleCounters[handle & HANDLE_ID_MASK];
        WorkerFiber* currentFiber = GetWorker()->currentFiber;

        JobWaitor waitor = {};
        waitor.fiber = currentFiber;
        waitor.next = counter.waitor;
        counter.waitor = &waitor;
            
        // Get free fiber
        WorkerFiber* newFiber = gManager->freeFibers.back();
        gManager->freeFibers.pop_back();
        if (!Fiber::IsValid(newFiber->handle))
            newFiber->handle = Fiber::Create(64 * 1024, FiberFunc, newFiber);

        GetWorker()->currentFiber = newFiber;
        Fiber::SwitchTo(currentFiber->handle, newFiber->handle);
        GetWorker()->currentFiber = currentFiber;
        gManager->sync.Unlock();
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

                // Worker
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
                currentFiber->currentJob = job;
                job.task(job.data);
                currentFiber->currentJob.task = nullptr;

                if (gManager->IsHandleValid(job.finishHandle))
                    gManager->Trigger(job.finishHandle);

                worker = GetWorker();
            }
        }

        Fiber::SwitchTo(currentFiber->handle, worker->primaryFiber);
    }
}
}

#pragma warning (pop)