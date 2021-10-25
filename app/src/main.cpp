

#include "test.h"

int main()
{
    std::unique_ptr<TestApp> app = std::make_unique<TestApp>();
    if (app == nullptr)
        return 0;

    std::unique_ptr<Platform> platform = std::make_unique<Platform>();
    if (!platform->Init(800, 600))
        return 0;

    if (!app->Initialize(std::move(platform)))
        return 0;

    while (app->Poll())
        app->Tick();

    app->Uninitialize();
    app.reset();

    return 0;
}