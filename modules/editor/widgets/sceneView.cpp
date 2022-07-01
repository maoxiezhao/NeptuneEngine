#include "sceneView.h"
#include "editor\editor.h"
#include "math\vMath_impl.hpp"
#include "renderer\renderScene.h"

#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	void EditorRenderer::Update(F32 dt)
	{
		RenderScene* scene = GetScene();
		ASSERT(scene != nullptr);

		RenderPath2D::Update(dt);

		// Update main camera
		ASSERT(camera);
		camera->width = (F32)currentBufferSize.x;
		camera->height = (F32)currentBufferSize.y;
		camera->UpdateCamera();

		// Culling for main camera
		visibility.Clear();
		visibility.camera = camera;
		visibility.flags = Visibility::ALLOW_EVERYTHING;
		scene->UpdateVisibility(visibility);

		// Update per frame data
		Renderer::UpdateFrameData(visibility, *scene, dt);
	}

	void EditorRenderer::Render()
	{
		UpdateRenderData();

		GPU::DeviceVulkan* device = wsi->GetDevice();
		auto& graph = GetRenderGraph();
		graph.SetupAttachments(*device, nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); // Used for sceneView
		Jobsystem::JobHandle handle;
		graph.Render(*device, handle);
		Jobsystem::Wait(&handle);
	}

	struct WorldViewImpl final : WorldView
	{
		SceneView& sceneView;
		EditorApp& editor;
		WorldEditor& worldEditor;
		CameraComponent camera;
		Transform transform;

		F32 deltaTime = 0.0f;
		F32x2 mousePos{0.f, 0.f};
		F32x2 mouseSensitivity{ 200.f, 200.f };

	public:
		WorldViewImpl(SceneView& view_, EditorApp& editor_) :
			sceneView(view_),
			editor(editor_),
			worldEditor(editor_.GetWorldEditor())
		{
			camera.up = F32x3(0.0f, 1.0f, 0.0f);
			camera.eye = F32x3(0.0f, 0.0f, 0.0f);
			camera.at = F32x3(0.0f, 0.0f, 1.0f);
			camera.width = -1.f;
			camera.height = -1.f;
		}

		virtual ~WorldViewImpl()
		{
		}

		void OnMouseMove(int x, int y, int relx, int rely)
		{
			PROFILE_FUNCTION();
			mousePos = F32x2((F32)x, (F32)y);

			const float yaw = Signum(relx) * (powf(fabsf((float)relx / mouseSensitivity.x), 1.2f));
			const float pitch = Signum(rely) * (powf(fabsf((float)rely / mouseSensitivity.y), 1.2f));
			transform.RotateRollPitchYaw(F32x3(pitch, yaw, 0.0f));
			transform.UpdateTransform();
		}

		void OnCameraMove(float forward, float right, float up, float speed)
		{
			F32x3 dir = F32x3(0.0f);
			dir.x += right;
			dir.y += up;
			dir.z += forward;
			dir *= speed;
			MATRIX rot = MatrixRotationQuaternion(LoadF32x4(transform.rotation));
			VECTOR move = Vector3TransformNormal(LoadF32x3(dir), rot);

			transform.Translate(StoreF32x3(move));
			transform.UpdateTransform();
		}

		void Update(F32 delta)
		{
			deltaTime = delta;
			transform.UpdateTransform();
			camera.UpdateTransform(transform);
		}
	};

	SceneView::SceneView(EditorApp& app_) :
		app(app_),
		worldEditor(app_.GetWorldEditor()),
		moveForwardAction("Move forward", "moveForward"),
		moveBackAction("Move back", "moveBack"),
		moveLeftAction("Move left", "moveLeft"),
		moveRightAction("Move right", "moveRight")
	{
		app.AddAction(&moveForwardAction);
		app.AddAction(&moveBackAction);
		app.AddAction(&moveLeftAction);
		app.AddAction(&moveRightAction);
	}

	SceneView::~SceneView()
	{
		app.RemoveAction(&moveForwardAction);
		app.RemoveAction(&moveBackAction);
		app.RemoveAction(&moveLeftAction);
		app.RemoveAction(&moveRightAction);

		CJING_SAFE_DELETE(worldView);
		editorRenderer.Reset();
	}

	void SceneView::Init()
	{
		worldView = CJING_NEW(WorldViewImpl)(*this, app);

		editorRenderer = CJING_MAKE_UNIQUE<EditorRenderer>();
		editorRenderer->SetWSI(&app.GetEngine().GetWSI());
		editorRenderer->DisableSwapchain();

		auto world = worldEditor.GetWorld();
		ASSERT(world);

		RenderScene* scene = dynamic_cast<RenderScene*>(world->GetScene("Renderer"));
		ASSERT(scene);
		editorRenderer->SetScene(scene);
	}

	void SceneView::Update(F32 dt)
	{
		PROFILE_FUNCTION();
		worldView->Update(dt);

		if (ImGui::IsAnyItemActive()) return;
		if (!isMouseCaptured) return;
		if (ImGui::GetIO().KeyCtrl) return;

		float speed = cameraSpeed * dt * 60.f;
		// Move camera
		if (moveForwardAction.IsActive()) worldView->OnCameraMove(1.0f, 0, 0, speed);
		if (moveBackAction.IsActive()) worldView->OnCameraMove(-1.0f, 0, 0, speed);
		if (moveLeftAction.IsActive()) worldView->OnCameraMove(0.0f, -1.0f, 0, speed);
		if (moveRightAction.IsActive()) worldView->OnCameraMove(0.0f, 1.0f, 0, speed);
	}

	void SceneView::EndFrame()
	{
	}

	void SceneView::OnGUI()
	{
		PROFILE_FUNCTION();

		ImVec2 viewPos;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		if (ImGui::Begin("Scene View", nullptr, ImGuiWindowFlags_NoScrollWithMouse))
		{
			const ImVec2 size = ImGui::GetContentRegionAvail();
			if (size.x <= 0 || size.y <= 0)
			{
				ImGui::End();
				ImGui::PopStyleVar();
				return;
			}

			editorRenderer->SetCamera(&worldView->camera);

			const ImVec2 mousePos = ImGui::GetCursorScreenPos();
			screenPos.x = I32(mousePos.x);
			screenPos.y = I32(mousePos.y);
			screenSize.x = (I32)size.x;
			screenSize.y = (I32)size.y;

			U32x2 viewportSize = editorRenderer->GetViewportSize();
			if ((U32)size.x != viewportSize.x || (U32)size.y != viewportSize.y)
			{
				editorRenderer->SetViewportSize((U32)size.x, (U32)size.y);
				editorRenderer->ResizeBuffers();
			}
		
			auto& graph = editorRenderer->GetRenderGraph();
			auto& backRes = graph.GetOrCreateTexture("back");
			auto backTex = graph.TryGetPhysicalTexture(&backRes);
			if (backTex)
				ImGui::Image(graph.GetPhysicalTexture(backRes).GetImage(), size);

			HandleEvents();

			// Maybe should call it in SceneView::Update()
			editorRenderer->Update(worldView->deltaTime);
		}

		if (isMouseCaptured && (Platform::GetFocusedWindow() != ImGui::GetWindowViewport()->PlatformHandle))
			CaptureMouse(false);

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void SceneView::Render()
	{
		editorRenderer->Render();
	}

	const char* SceneView::GetName()
	{
		return "SceneView";
	}

	void SceneView::HandleEvents()
	{
		const bool handleInput = isMouseCaptured || 
			(ImGui::IsItemHovered() && Platform::GetFocusedWindow() == ImGui::GetWindowViewport()->PlatformHandle);
		const auto& windowEvents = app.GetWindowEvents();
		for (auto& ent : windowEvents)
		{
			switch (ent.type)
			{
				case Platform::WindowEvent::Type::MOUSE_BUTTON:
				{
					I32x2 relMP = GetLocalMousePoint();
					if (handleInput)
					{
						if (ent.mouseButton.button == Platform::MouseButton::RIGHT) 
						{
							ImGui::SetWindowFocus();
							CaptureMouse(ent.mouseButton.down);
						}
					}
					break;
				}
				case Platform::WindowEvent::Type::MOUSE_MOVE:
				{
					if (handleInput && isMouseCaptured)
					{
						I32x2 relMP = GetLocalMousePoint();
						worldView->OnMouseMove((I32)relMP.x, (I32)relMP.y, (I32)ent.mouseMove.xrel, (I32)ent.mouseMove.yrel);
					}
					break;
				}
			}
		}
	}

	void SceneView::CaptureMouse(bool capture)
	{
		if (isMouseCaptured == capture)
			return;

		isMouseCaptured = capture;
		app.SetCursorCaptured(capture);
		Platform::SetMouseCursorVisible(!capture);

		if (capture)
		{
			Platform::GrabMouse((Platform::WindowType)ImGui::GetWindowViewport()->PlatformHandle);
			auto pos = Platform::GetMouseScreenPos();
			capturedMousePos.x = pos.x;
			capturedMousePos.y = pos.y;
		}
		else
		{
			Platform::GrabMouse(nullptr);
			Platform::SetMouseScreenPos(capturedMousePos.x, capturedMousePos.y);
		}
	}

	I32x2 SceneView::GetLocalMousePoint() const
	{
		const auto cp = Platform::GetMouseScreenPos();
		return I32x2(cp.x - screenPos.x, cp.y - screenPos.y);
	}
}
}