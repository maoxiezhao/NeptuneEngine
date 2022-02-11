#include "core\jobsystem\jobsystem.h"
#include "core\platform\platform.h"
#include "ecs\ecs.h"

using namespace VulkanTest;

int main()
{
	if (!Jobsystem::Initialize(VulkanTest::Platform::GetCPUsCount() - 1))
    {
        std::cout << "Failed to init jobsystem" << std::endl;
        return 0;
    }

    Jobsystem::Uninitialize();
	return 0;
}