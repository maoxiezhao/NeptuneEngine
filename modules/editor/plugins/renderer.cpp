#include "renderer.h"
#include "editor\editor.h"
#include "editor\widgets\assetCompiler.h"

namespace VulkanTest
{
namespace Editor
{
	class TestResource final : public Resource
	{
	public:
		DECLARE_RESOURCE(TestResource);

		TestResource(const Path& path_, ResourceFactory& resFactory_) :
			Resource(path_, resFactory_)
		{
		}

		virtual ~TestResource()
		{
		}

	protected:
		bool OnLoaded(U64 size, const U8* mem) override
		{
			ASSERT(mem != nullptr);
			ASSERT(size > 0);
			data = (U8*)CJING_MALLOC(size);
			memcpy(data, mem, size);
			return true;
		}

		void OnUnLoaded() override
		{
			CJING_SAFE_FREE(data);
		}

	private:
		U8* data = nullptr;
	};

	DEFINE_RESOURCE(TestResource);

	class TestResourceFactory : public ResourceFactory
	{
	protected:
		Resource* CreateResource(const Path& path)override
		{
			return CJING_NEW(TestResource)(path, *this);
		}

		virtual void DestroyResource(Resource* res)override
		{
			CJING_SAFE_DELETE(res);
		}
	};

	struct TestPlugin final : AssetCompiler::IPlugin
	{
	private:
		EditorApp& app;

	public:
		TestPlugin(EditorApp& app_) : app(app_) 
		{
			app_.GetAssetCompiler().RegisterExtension("txt", TestResource::ResType);
		}

		bool Compile(const Path& path)
		{
			return app.GetAssetCompiler().CopyCompile(path);
		}
	};

	struct RenderPlugin : EditorPlugin
	{
	private:
		EditorApp& app;

		TestPlugin testPlugin;
		TestResourceFactory testResFactory;

	public:
		RenderPlugin(EditorApp& app_) :
			app(app_),
			testPlugin(app_)
		{
		}

		virtual ~RenderPlugin() 
		{
			testResFactory.Uninitialize();
		}

		void Initialize() override
		{
			ResourceManager& resManager = app.GetEngine().GetResourceManager();
			testResFactory.Initialize(TestResource::ResType, resManager);

			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.AddPlugin(testPlugin, "txt");
		}

		const char* GetName()const override
		{
			return "renderer";
		}
	};

	EditorPlugin* SetupPluginRenderer(EditorApp& app)
	{
		return CJING_NEW(RenderPlugin)(app);
	}
}
}
