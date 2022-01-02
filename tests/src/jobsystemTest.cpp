#include "client\app\app.h"
#include "gpu\vulkan\device.h"
#include "math\math.hpp"
#include "core\jobsystem\jobsystem.h"
#include "core\platform\platform.h"

#include <coroutine>

using namespace VulkanTest;

int main()
{
	if (!Jobsystem::Initialize(VulkanTest::Platform::GetCPUsCount()))
    {
        std::cout << "Failed to init jobsystem" << std::endl;
        return 0;
    }

    Jobsystem::ShowDebugInfo();

    struct TestData
    {
        int value = 0;
    } 
    testData;

    Jobsystem::JobHandle handle = Jobsystem::INVALID_HANDLE;
    for(int i = 0; i < 20; i++)
    { 
        Jobsystem::JobInfo jobInfo = {};
        jobInfo.name = "Test";
        jobInfo.priority = Jobsystem::Priority::NORMAL;
        jobInfo.precondition = Jobsystem::INVALID_HANDLE;
        jobInfo.data = &testData;
        jobInfo.jobFunc = [](void* data) {
            TestData* testData = reinterpret_cast<TestData*>(data);
            testData->value++;
            Platform::Sleep(1);
            return;
        };
        Jobsystem::Run(jobInfo, &handle);
    }

    Jobsystem::Wait(handle);

    std::cout << "Test value:" << testData.value << std::endl;

    Jobsystem::ShowDebugInfo();
    Jobsystem::Uninitialize();
	return 0;
}