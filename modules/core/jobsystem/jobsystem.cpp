#include "jobsystem.h"
#include "core\memory\memory.h"
#include "core\platform\fiber.h"
#include "core\platform\platform.h"
#include "core\platform\sync.h"
#include "core\platform\atomic.h"
#include "core\profiler\profiler.h"

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
        JobHandle* onFinishedHandle;
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

    static volatile I32 gGeneration = 0;

    struct ManagerImpl
    {
        Mutex sync;
        Mutex jobQueueLock;

        std::vector<WorkerFiber*> readyFibers;
        std::vector<WorkerFiber*> freeFibers;
        WorkerFiber fiberPool[MAX_FIBER_COUNT];
        std::vector<WorkerThread*> workers;
        std::vector<JobImpl> jobQueue;
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
        ManagerImpl& manager;
        U32 workderIndex;
        bool isFinished = false;
        bool isEnabled = false;

        WorkerFiber* currentFiber = nullptr;
        Fiber::Handle primaryFiber = Fiber::INVALID_HANDLE;
        std::vector<WorkerFiber*> readyFibers;
        std::vector<JobImpl> jobQueue;

    public:
        WorkerThread(ManagerImpl& manager_, U32 workerIndex_) :
            manager(manager_),
            workderIndex(workerIndex_)
        {
        }

        int Task() override
        {
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

    //////////////////////////////////////////////////////////////
    // Methods

    bool Initialize(U32 numWorkers)
    {
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
            StaticString<32> worldName;
            WorkerThread* worker = CJING_NEW(WorkerThread)(*gManager, i);
            if (worker->Create(worldName.Sprintf("Worker_%d", i).c_str()))
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
            worker->isFinished = true;

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
        job.onFinishedHandle = handle;

        if (handle != nullptr)
        {
            ScopedMutex guard(gManager->sync);
            handle->counter++;
            if (handle->counter == 1)
                handle->generation = AtomicIncrement(&gGeneration);
        }

        // Push job for worker
        if (job.workerIndex != ANY_WORKER)
        {
            WorkerThread* worker = gManager->workers[job.workerIndex % gManager->workers.size()];
            {
                ScopedMutex lock(gManager->jobQueueLock);
                worker->jobQueue.push_back(job);
            }
            worker->Wakeup();
        }
        else
        {
            {
                ScopedMutex lock(gManager->jobQueueLock);
                gManager->jobQueue.push_back(job);
            }

            for (auto worker : gManager->workers)
                worker->Wakeup();
        }
    }

    void Run(void*data, JobFunc func, JobHandle* handle, U8 workerIndex)
    {
        ASSERT(gManager.Get() != nullptr);

        RunInternal(
            func,
            data,
            handle,
            workerIndex
        );
    }

    void Wait(JobHandle* handle)
    {
        ASSERT(gManager.Get() != nullptr);

        if (handle == nullptr || handle->counter == 0)
            return;

        gManager->sync.Lock();
        if (handle->counter == 0)
        {
            gManager->sync.Unlock();
            return;
        }
    
        // No worker, just sleep
        if (GetWorker() == nullptr)
        {
            while (handle->counter > 0)
            {
                gManager->sync.Unlock();
                Platform::Sleep(1);
                gManager->sync.Lock();
            }
            gManager->sync.Unlock();
            return;
        }

        // Set the current fiber as the next waitor of the pending handle
        WorkerFiber* thisFiber = GetWorker()->currentFiber;

        JobWaitor waitor = {};
        waitor.fiber = thisFiber;
        waitor.next = handle->waitor;
        handle->waitor = &waitor;
        
        auto switchData = Profiler::BeginFiberWait();

        // Get free fiber
        WorkerFiber* newFiber = gManager->freeFibers.back();
        gManager->freeFibers.pop_back();
        if (!Fiber::IsValid(newFiber->handle))
            newFiber->handle = Fiber::Create(64 * 1024, FiberFunc, newFiber);

        GetWorker()->currentFiber = newFiber;
        Fiber::SwitchTo(thisFiber->handle, newFiber->handle);
        GetWorker()->currentFiber = thisFiber;
        gManager->sync.Unlock();

        Profiler::EndFiberWait(switchData);
    }

    bool Trigger(JobHandle* jobHandle)
    {
        JobWaitor* waitor = nullptr;
        {
            ScopedMutex lock(gManager->sync);
            jobHandle->counter--;
            ASSERT(jobHandle->counter >= 0);
            if (jobHandle->counter > 0)
                return false;

            waitor = jobHandle->waitor;
            jobHandle->waitor = nullptr;
        }

        if (waitor == nullptr)
            return false;

        bool needWakeAll = false;
        {
            ScopedMutex lock(gManager->jobQueueLock);
            while (waitor != nullptr)
            {
                JobWaitor* next = waitor->next;
                U8 workerIndex = waitor->fiber->currentJob.workerIndex; 
                if (workerIndex == ANY_WORKER)
                {
                    gManager->readyFibers.push_back(waitor->fiber);
                    needWakeAll = true;

                }
                else
                {
                    WorkerThread* worker = gManager->workers[workerIndex % gManager->workers.size()];
                    worker->readyFibers.push_back(waitor->fiber);
                    if (!needWakeAll)
                        worker->Wakeup();
                }
                waitor = next;
            }
        }

        if (needWakeAll)
        {
            for (auto worker : gManager->workers)
                worker->Wakeup();
        }

        return true;
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
            WorkerFiber* fiber = nullptr;
            JobImpl job;
            while (!worker->isFinished)
            {
                ScopedMutex lock(gManager->jobQueueLock);

                // Worker
                if (!worker->readyFibers.empty())
                {
                    fiber = worker->readyFibers.back();
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
                    fiber = gManager->readyFibers.back();
                    gManager->readyFibers.pop_back();
                    break;
                }
                if (!gManager->jobQueue.empty())
                {
                    job = gManager->jobQueue.back();
                    gManager->jobQueue.pop_back();
                    break;
                }

                // PROFILE_BLOCK("Sleeping");
                worker->Sleep(gManager->jobQueueLock);
            }

            if (worker->isFinished)
                break;

            if (fiber != nullptr)
            {
                // Do ready fiber
                worker->currentFiber = fiber;

                gManager->sync.Lock();
                gManager->freeFibers.push_back(currentFiber);
                Fiber::SwitchTo(currentFiber->handle, fiber->handle);
                gManager->sync.Unlock();

                worker = GetWorker();
                worker->currentFiber = currentFiber;
            }
            else if (job.task != nullptr)
            {
                Profiler::BeginBlock("Job");

                // Do target job
                currentFiber->currentJob = job;
                job.task(job.data);
                currentFiber->currentJob.task = nullptr;

                if (job.onFinishedHandle)
                    Trigger(job.onFinishedHandle);

                worker = GetWorker();
                Profiler::EndBlock();
            }
        }

        Fiber::SwitchTo(currentFiber->handle, GetWorker()->primaryFiber);
    }
}
}

#pragma warning (pop)