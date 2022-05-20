#include "worldEditor.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
    class WorldEditorImpl : public WorldEditor
    {
    private:
        EditorApp& editor;
        Engine& engine;
        World* world = nullptr;

    public:
        WorldEditorImpl(EditorApp& editor_) :
            editor(editor_),
            engine(editor_.GetEngine())
        {
            NewWorld();
        }

        ~WorldEditorImpl()
        {
            if (world != nullptr)
                DestroyWorld();
        }

        void Update(F32 dt)override
        {
        }

        void EndFrame()override
        {
        }

        void OnGUI()override
        {
        }

        const char* GetName()override
        {
            return "WorldEditor";
        }

        World* GetWorld()override
        {
            return world;
        }

        void NewWorld()override
        {
            ASSERT(world == nullptr);           
            world = &engine.CreateWorld();
        }

        void DestroyWorld()override
        {
            ASSERT(world != nullptr);
            engine.DestroyWorld(*world);
            world = nullptr;
        }
    };

    UniquePtr<WorldEditor> WorldEditor::Create(EditorApp& app)
    {
        return CJING_MAKE_UNIQUE<WorldEditorImpl>(app);
    }
}
}