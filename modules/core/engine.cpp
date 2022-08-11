#include "engine.h"

namespace VulkanTest
{
#define DEFINE_ENGINE_SERVICE(name) \
    void EngineService::name() { } \
    void EngineService::On##name() \
    { \
        auto& services = GetServices(); \
        for (I32 i = 0; i < services.size(); i++) \
            services[i]->name(); \
    }

	DEFINE_ENGINE_SERVICE(FixedUpdate);
	DEFINE_ENGINE_SERVICE(Update);
	DEFINE_ENGINE_SERVICE(LateUpdate);

	EngineService::~EngineService()
	{
	}

	EngineService::ServicesArray& EngineService::GetServices()
	{
		static ServicesArray Services;
		return Services;
	}

	EngineService::EngineService(const char* name_) :
		name(name_)
	{
		GetServices().push_back(this);
	}

    bool EngineService::Init()
    {
        return false;
    }

    void EngineService::OnInit()
    {
        // Init services from front to back
        auto& services = GetServices();
        for (I32 i = 0; i < services.size(); i++)
        {
            const auto service = services[i];
            Logger::Info("Initialize %s...", service->name);
            service->initialized = true;
            if (!service->Init())
            {
                Logger::Error("Failed to initialize %s.", service->name);
                return;
            }
        }
    }

    void EngineService::Uninit()
    {
    }

    void EngineService::OnUninit()
    {
        auto& services = GetServices();
        for (I32 i = services.size() - 1; i >= 0; i--)
        {
            const auto service = services[i];
            if (service->initialized)
            {
                service->initialized = false;
                service->Uninit();
            }
        }
    }
}