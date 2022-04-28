#include "editor.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorAppImpl final : public EditorApp
    {
    public:
        EditorAppImpl()
        {

        }

        ~EditorAppImpl()
        {
            // Clear widgets
            for(EditorWidget* plugin : widgets)
                CJING_SAFE_DELETE(plugin);
            widgets.clear();

            // Clear editor plugins
            for(EditorPlugin* plugin : plugins)
                CJING_SAFE_DELETE(plugin);
            plugins.clear();
        }

        void AddPlugin(EditorPlugin& plugin) override
        {
            plugins.push_back(&plugin);
        }

        void AddWidget(EditorWidget& widget) override
        {
            widgets.push_back(&widget);
        }

        void RemoveWidget(EditorWidget& widget) override
        {
            widgets.swapAndPopItem(&widget);
        }

    private:
        Array<EditorPlugin*> plugins;
        Array<EditorWidget*> widgets;
    };

    EditorApp* EditorApp::Create()
    {
		return CJING_NEW(EditorAppImpl)();
    }
    
    void EditorApp::Destroy(EditorApp* app)
    {
        CJING_SAFE_DELETE(app);
    }
}
}