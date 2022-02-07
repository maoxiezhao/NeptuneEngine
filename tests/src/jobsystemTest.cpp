#include "client\app\app.h"
#include "gpu\vulkan\device.h"
#include "math\math.hpp"
#include "core\jobsystem\jobsystem.h"
#include "core\platform\platform.h"

#include <coroutine>
#include <chrono>

using namespace VulkanTest;

int main()
{
	if (!Jobsystem::Initialize(VulkanTest::Platform::GetCPUsCount()))
    {
        std::cout << "Failed to init jobsystem" << std::endl;
        return 0;
    }

   /* Jobsystem::ShowDebugInfo();

    struct TestData
    {
        int value = 0;
    } 
    testData;

    auto time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    Jobsystem::JobHandle handle = Jobsystem::INVALID_HANDLE;
    for(int i = 0; i < 200; i++)
    { 
        Jobsystem::JobInfo jobInfo = {};
        jobInfo.name = "Test";
        jobInfo.priority = Jobsystem::Priority::NORMAL;
        jobInfo.precondition = Jobsystem::INVALID_HANDLE;
        jobInfo.data = &testData;
        jobInfo.jobFunc = [](void* data) {
            TestData* testData = reinterpret_cast<TestData*>(data);
            
            Jobsystem::JobHandle handle = Jobsystem::INVALID_HANDLE;
            Jobsystem::Run(testData, [](void* data) {
                TestData* testData = reinterpret_cast<TestData*>(data);
                testData->value-=2;
            }, & handle);
            Jobsystem::Wait(handle);

            testData->value++;
            return;
        };
        Jobsystem::Run(jobInfo, &handle);
    }

    Jobsystem::Wait(handle);
    time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time;
    std::cout << "Test value:" << testData.value << std::endl;
    std::cout << "Wait time:" << time << std::endl;
    Jobsystem::ShowDebugInfo();*/
    Jobsystem::Uninitialize();
	return 0;
}