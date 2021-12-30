#include "client\app\app.h"
#include "gpu\vulkan\device.h"
#include "math\math.hpp"
#include "core\jobsystem\jobsystem.h"
#include "core\platform\platform.h"

#include <coroutine>

using namespace VulkanTest;

int main()
{
    std::cout << (9.8f * 9.8f) << std::endl;

	/*if (Jobsystem::Initialize(VulkanTest::Platform::GetCPUsCount()))
    {
        std::cout << "Failed to init jobsystem" << std::endl;
        return 0;
    }

    struct TestData
    {
        int value = 0;
    } 
    testData;

    Jobsystem::JobInfo jobInfo = {};
    jobInfo.name = "Test";
    jobInfo.priority = Jobsystem::Priority::NORMAL;
    jobInfo.precondition = Jobsystem::INVALID_HANDLE;
    jobInfo.data = &testData;
    jobInfo.jobFunc = [](void* data) {
        TestData* testData = reinterpret_cast<TestData*>(data);
        for(int i = 0; i < 100; i++)
        {
            testData->value++;
        }
        return;
    };
    Jobsystem::JobHandle handle = Jobsystem::INVALID_HANDLE;
    Jobsystem::Run(jobInfo, &handle);
    Jobsystem::Wait(handle);

    std::cout << "Test value:" << testData.value << std::endl;

    Jobsystem::Uninitialize();*/
	return 0;
}