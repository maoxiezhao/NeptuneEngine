#include "renderGraph.h"
#include "editor\editor.h"
#include "editor\widgets\sceneView.h"
#include "renderer\renderGraph.h"
#include "imgui-docking\imgui.h"
#include "imnodes\imnodes.h"

namespace VulkanTest
{
namespace Editor
{
	struct NodeIDGrenerator
	{
		I32 id = 1;
		I32 Generate() {
			return id++;
		}

		void Reset() {
			id = 0;
		}
	};

	enum class NodeType
	{
		RenderPass,
		Backbuffer
	};

	struct UINode
	{
		String name;
		I32 id = -1;
		NodeType type = NodeType::RenderPass;
		virtual void OnNodeGUI() = 0;
	};

	struct AttrNode
	{
		String name;
		I32 id = -1;
		U64 hash;
	};

	const ImVec2 NODE_BASE_POS = ImVec2(100, 50);
	const ImVec2 NODE_MARIGIN = ImVec2(200, 150);

	struct RenderPassNode : UINode
	{
		bool firstLoad = true;
		Array<AttrNode> reads;
		Array<AttrNode> writes;
		U32 depth = 0;
		U32 row = 0;

		void OnNodeGUI()
		{
			const float node_width = 100.0f;
			switch (type)
			{
			case NodeType::RenderPass:
				ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(11, 109, 191, 255));
				ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(45, 126, 194, 255));
				ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(81, 148, 204, 255));
				break;

			case NodeType::Backbuffer:
				ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(109, 10, 10, 255));
				ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(140, 20, 15, 255));
				ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(160, 15, 16, 255));
				break;
			default:
				break;
			}

			ImNodes::BeginNode(id);

			ImNodes::BeginNodeTitleBar();
			ImGui::TextUnformatted(name.c_str());
			ImNodes::EndNodeTitleBar();

			ImGui::Dummy(ImVec2(node_width, 0.f));
			for (const auto& attr : reads)
			{
				ImNodes::BeginInputAttribute(attr.id);
				ImGui::TextUnformatted(attr.name);
				ImNodes::EndInputAttribute();
				ImGui::Spacing();
			}

			for (const auto& attr : writes)
			{
				ImNodes::BeginOutputAttribute(attr.id);
				const float label_width = ImGui::CalcTextSize(attr.name.c_str()).x;
				ImGui::Indent(node_width - label_width);
				ImGui::TextUnformatted(attr.name);
				ImNodes::EndOutputAttribute();
				ImGui::Spacing();
			}

			ImNodes::EndNode();

			ImNodes::PopColorStyle();
			ImNodes::PopColorStyle();
			ImNodes::PopColorStyle();

			if (firstLoad)
			{
				firstLoad = false;
				ImNodes::SetNodeGridSpacePos(id, {
					NODE_BASE_POS.x + depth * NODE_MARIGIN.x,
					NODE_BASE_POS.y + row * NODE_MARIGIN.y
				});
			}
		}
	};

	class RenderGraphWidgetImpl : public RenderGraphWidget
	{
	private:
		EditorApp& editor;
		SceneView* sceneView = nullptr;
		NodeIDGrenerator idGenerator;
		NodeIDGrenerator edgeIDGenerator;
		Array<RenderPassNode> renderPassNodes;
		HashMap<U64, I32> renderPassOutputIDs;
		HashMap<U64, Array<I32>> renderPassInputIDs;
		HashMap<U64, U32> renderPassDepth;

	public:
		RenderGraphWidgetImpl(EditorApp& editor_) :
			editor(editor_)
		{
			ImNodes::CreateContext();
			ImNodes::StyleColorsDark();
		}

		~RenderGraphWidgetImpl()
		{
			ImNodes::DestroyContext();
		}

		void InitFinished() override
		{
			sceneView = (SceneView*)editor.GetWidget("SceneView");
			ASSERT(sceneView);
		}

		void Update(F32 dt)override
		{
			if (sceneView == nullptr)
				return;

			RenderGraph& renderGraph = sceneView->GetRenderer().GetRenderGraph();
			if (renderGraph.IsBaked())
				OnRenderGraphBaked();
		}

		void OnGUI()override
		{
			if (!isOpen) return;

			if (ImGui::Begin("RenderGraph##renderGraph", &isOpen))
			{
				RenderGraph& renderGraph = sceneView->GetRenderer().GetRenderGraph();

				if (ImGui::Button("Log"))
					renderGraph.Log();
				ImGui::SameLine();
				if (ImGui::Button("Graphivz"))
					renderGraph.ExportGraphviz();

				ImNodes::BeginNodeEditor();

				for (auto& node : renderPassNodes)
					node.OnNodeGUI();

				for (auto& node : renderPassNodes)
				{
					for (auto& attr : node.writes)
					{
						I32 from = attr.id;
						auto it = renderPassInputIDs.find(attr.hash);
						if (it.isValid())
						{
							for (auto& to : it.value())
								ImNodes::Link(edgeIDGenerator.Generate(), from, to);
						}
							
					}
				}

				ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);
				ImNodes::EndNodeEditor();
			}
			ImGui::End();
		}

		const char* GetName()override
		{
			return "RenderGraphWidget";
		}

		void OnRenderGraphBaked()
		{
			idGenerator.Reset();
			edgeIDGenerator.Reset();
			renderPassNodes.clear();
			renderPassInputIDs.clear();
			renderPassOutputIDs.clear();
			renderPassDepth.clear();

			RenderGraph& renderGraph = sceneView->GetRenderer().GetRenderGraph();
			std::vector<DebugRenderPassInfo> infos;
			renderGraph.GetDebugRenderPassInfos(infos);

			for (auto& info : infos)
				CreateRenderPassNode(info);

			// Add backbuffer
			DebugRenderPassInfo info = {};
			info.name = "Backbuffer";
			info.reads.push_back(renderGraph.GetBackBufferSource());
			CreateRenderPassNode(info, NodeType::Backbuffer);

			UpdateRenderPassDepth();
		}

		RenderPassNode& CreateRenderPassNode(const DebugRenderPassInfo& info, const NodeType type = NodeType::RenderPass)
		{
			RenderPassNode& newNode = renderPassNodes.emplace();
			newNode.id = idGenerator.Generate();
			newNode.name = info.name;
			newNode.type = type;

			for (auto& name : info.reads)
			{
				I32 id = idGenerator.Generate();
				U64 hash = StringID(name.c_str()).GetHashValue();
				newNode.reads.push_back({ name.c_str(), id, hash });

				auto it = renderPassInputIDs.find(hash);
				if (!it.isValid())
					it = renderPassInputIDs.insert(hash, {});

				it.value().push_back(id);
			}

			for (auto& name : info.writes)
			{
				I32 id = idGenerator.Generate();
				U64 hash = StringID(name.c_str()).GetHashValue();
				newNode.writes.push_back({ name.c_str(), id, hash });
				renderPassOutputIDs.insert(hash, id);
			}

			return newNode;
		}
	
		void UpdateRenderPassDepth()
		{
			I32 depthMap[64];
			memset(depthMap, 0, sizeof(depthMap));

			for (auto& node : renderPassNodes)
			{
				U32 depth = 0;
				for (auto& attr : node.reads)
				{
					auto it = renderPassDepth.find(attr.hash);
					if (it.isValid())
						depth = std::max(depth, it.value() + 1);
				}
				node.depth = depth;
				node.row = depthMap[depth]++;

				for (auto& attr : node.writes)
				{
					auto it = renderPassDepth.find(attr.hash);
					auto curDepth = it.isValid() ? it.value() : 0;
					renderPassDepth.insert(attr.hash, std::max(curDepth, depth));
				}
			}
		}
	};
	
	UniquePtr<RenderGraphWidget> RenderGraphWidget::Create(EditorApp& app)
	{
		return CJING_MAKE_UNIQUE<RenderGraphWidgetImpl>(app);
	}
}
}


