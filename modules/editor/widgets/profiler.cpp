#include "profiler.h"
#include "editor\editor.h"
#include "editor\profiler\profilerTools.h"
#include "editor\profiler\samplesBuffer.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	struct ProfilerData
	{
		ProfilerTools::MainStats mainStats;
		std::vector<ProfilerTools::ThreadStats> cpuThreads;
	};

	// Base profiler mode
	struct ProfilerMode
	{
		static const I32 MaxSamples = 60;

		bool showLastUpdateBlocks = false;
		bool showMainThreadOnly = false;

		virtual ~ProfilerMode() {}
		virtual void Update(ProfilerData& tools) = 0;
		virtual void OnGUI(bool isPaused) = 0;
		virtual void Clear() = 0;
		virtual void PrevFrame() {}
		virtual void NextFrame() {}
	};

	// Draws simple chart.
	struct SingleChart
	{
	private:
		String name;
		SamplesBuffer<F32, ProfilerMode::MaxSamples> samples;
		int selectedSampleIndex = -1;

		const F32 HEIGHT = 60.0f;

	public:
		SingleChart(const char* name_) :
			name(name_)
		{
		}

		void AddSample(F32 value)
		{
			samples.Add(value);
		}

		F32 GetSample(I32 index)const {
			return samples.Get(index);
		}

		I32 GetSelectedSampleIndex()const {
			return selectedSampleIndex;
		}

		void SetSelectedSampleIndex(I32 index) {
			selectedSampleIndex = index;
		}

		void OnGUI()
		{
			if (samples.Count() <= 0)
				return;

			ImGui::Text(name.c_str());

			// Calculate min value and max value
			F32* datas = samples.Data();
			F32 minValue = datas[samples.Count() - 1];
			F32 maxValue = datas[samples.Count() - 1];
			for (int i = 0; i < samples.Count() - 1; i++)
			{
				minValue = std::min(minValue, datas[i]);
				maxValue = std::max(maxValue, datas[i]);
			}
			minValue *= 0.95f;
			maxValue *= 1.05f;

			// Draw selected line
			F32 WIDTH = ImGui::GetContentRegionAvail().x;
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 screenPos = ImGui::GetCursorScreenPos();
			F32 framePaddingX = ImGui::GetStyle().FramePadding.x;
			F32 innerWidth = (WIDTH - 2 * framePaddingX);
			ImVec2 innerPos = ImVec2(screenPos.x + framePaddingX, screenPos.x + WIDTH - framePaddingX);

			// Draw frame histogram
			ImGui::PlotHistogram("", samples.Data(), samples.Count(), 0, 0, minValue, maxValue, ImVec2(WIDTH, HEIGHT));

			if (selectedSampleIndex != -1)
			{
				F32 offset = innerWidth / samples.Count() * 0.5f;
				float selectedX = innerWidth * ((F32)selectedSampleIndex / (F32)samples.Count()) + offset;
				const ImVec2 ra(innerPos.x + selectedX, screenPos.y);
				const ImVec2 rb(innerPos.x + selectedX, screenPos.y + HEIGHT);
				dl->AddLine(ra, rb, 0xFFFFffff);
			}

			if (ImGui::IsMouseHoveringRect(ImVec2(innerPos.x, screenPos.y), ImVec2(innerPos.y, ImGui::GetCursorScreenPos().y)))
			{
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					const F32 x = std::max(0.0f, ImGui::GetMousePos().x - innerPos.x);
					selectedSampleIndex = (I32)(x / innerWidth * samples.Count());
				}
			}
		}

		void Clear()
		{
			samples.Clear();
		}
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	///  Overall profiler
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	struct OverallProfiler : ProfilerMode
	{
		SingleChart fpsChart;
		SingleChart updateTimesChart;

		OverallProfiler() :
			fpsChart("FPS"),
			updateTimesChart("UpdateTimes")
		{
		}

		void Update(ProfilerData& data) override
		{
			fpsChart.AddSample((F32)data.mainStats.fps);
			updateTimesChart.AddSample(data.mainStats.updateTimes);
		}

		void OnGUI(bool isPaused) override
		{
			if (!ImGui::BeginTabItem("Overall"))
				return;

			fpsChart.OnGUI();
			updateTimesChart.OnGUI();

			ImGui::EndTabItem();
		}

		void Clear() override
		{
			fpsChart.Clear();
			updateTimesChart.Clear();
		}
	};


	/////////////////////////////////////////////////////////////////////////////////////////////////////
	///  CPU profiler
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	struct CPUBlockNode
	{
		String name;
		F32 total;
		F32 time;
		I32 depth;

		static I32 DisplayNode(int index, int depth, const Array<CPUBlockNode>& nodes)
		{
			while (index < (I32)nodes.size())
			{
				const CPUBlockNode* node = &nodes[index];
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				I32 nextDepth = index < (I32)nodes.size() - 1 ? nodes[index + 1].depth : 0;
				if (nextDepth == depth + 1)
				{
					bool open = ImGui::TreeNodeEx(node->name, ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
					ImGui::TableNextColumn();
					ImGui::Text("%.2f%%", node->total);
					ImGui::TableNextColumn();
					ImGui::Text("%.4f", node->time);
					if (open)
					{
						index = DisplayNode(index + 1, depth + 1, nodes);
						ImGui::TreePop();
					}
				}
				else 
				{
					ImGui::TreeNodeEx(node->name, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
					ImGui::TableNextColumn();
					ImGui::Text("%.2f%%", node->total);
					ImGui::TableNextColumn();
					ImGui::Text("%.4f", node->time);

					if (nextDepth != depth)
						break;
				}
				index++;
			}

			return index;
		}
	};

	struct BlockRange
	{
		F64 startTime;
		F64 endTime;

		bool SkipBlock(const Profiler::Block& block)
		{
			return block.startTime < startTime || block.startTime > endTime;
		}
	};

	struct CPUProfiler : ProfilerMode
	{
	private:
		SingleChart updateTimesChart;
		SamplesBuffer<std::vector<ProfilerTools::ThreadStats>, ProfilerMode::MaxSamples> blocks;
		Array<CPUBlockNode> blockNodes;
		F64 timelineRange = 0.008f;
		I32 blockIndex = 0;
		U32 selectedFrame = 0;
		BlockRange currentRange;
		const U32 colors[6] = {
			0xFF44355B,
			0xFF51325C,
			0xFF1355B5,
			0xFF4365B2,
			0xFF156621,
			0xFF72A268,
		};

	public:
		CPUProfiler() :
			updateTimesChart("Frames")
		{
		}

		void Update(ProfilerData& data) override
		{
			updateTimesChart.AddSample(data.mainStats.updateTimes);
			blocks.Add(data.cpuThreads);

			OnSelectedFrameChagned(0, false);
		}
		
		void OnTableGUI(BlockRange range)
		{
			blockNodes.clear();

			if (blocks.Count() == 0)
				return;

			F64 totalTime = updateTimesChart.GetSample(0);
			auto blockData = blocks.Get(selectedFrame);
			if (blockData.empty())
				return;

			for (const auto& thread : blockData)
			{
				if (thread.blocks.empty())
					continue;

				if (showMainThreadOnly && thread.name != "AsyncMainThread")
					continue;

				for (const auto& block : thread.blocks)
				{
					F64 time = std::max(block.endTime - block.startTime, 0.000000001);
					if (block.endTime <= 0.0f || range.SkipBlock(block))
						continue;

					auto& node = blockNodes.emplace();
					auto pos = FindStringChar(block.name, ':', 0);
					node.name = pos >= 0 ? block.name + pos + 2 : block.name;
					node.total = (int)(time / totalTime * 1000.0f) / 10.0f;
					node.time = (F32)(time * 1000.0);
					node.depth = block.depth;
				}
			}

			static ImGuiTableFlags flags =
				ImGuiTableFlags_BordersV |
				ImGuiTableFlags_BordersOuterH |
				ImGuiTableFlags_Resizable |
				ImGuiTableFlags_RowBg |
				ImGuiTableFlags_NoBordersInBody;

			if (ImGui::BeginTable("FrameBlocks", 3, flags))
			{
				const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
				ImGui::TableSetupColumn("Block", ImGuiTableColumnFlags_NoHide);
				ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
				ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 18.0f);
				ImGui::TableHeadersRow();

				if (!blockNodes.empty())
					CPUBlockNode::DisplayNode(0, 0, blockNodes);

				ImGui::EndTable();
			}
		}

		F64 viewStart = 0.0f;
		F32 fromX = 0.0f;
		F32 fromY = 0.0f;
		F32 toX = 0.0f;
		void OnTimelineBlockGUI(const Profiler::Block& block, F32 baseY, F32 lineHeight, I32 index, I32 maxDepth, const std::vector<Profiler::Block>& blocks, ImDrawList* dl)
		{
			float posY = baseY + block.depth * lineHeight;
			auto DrawBlock = [&](F64 from, F64 to, const char* name, U32 color)
			{
				if (to < viewStart)
					return;

				const F32 tStart = F32((from - viewStart) / timelineRange);
				const F32 tEnd = F32((to - viewStart) / timelineRange);
				const F32 xStart = fromX * (1 - tStart) + toX * tStart;
				const F32 xEnd = fromX * (1 - tEnd) + toX * tEnd;

				// Block background
				const ImVec2 ra(xStart, posY);
				const ImVec2 rb(xEnd, posY + lineHeight - 1);
				dl->AddRectFilled(ra, rb, color);

				// Block border
				U32 borderColor = ImGui::GetColorU32(ImGuiCol_Border);
				F32 width = xEnd - xStart;
				if (width > 2) {
					dl->AddRect(ra, rb, borderColor);
				}

				// Block name text
				const float w = ImGui::CalcTextSize(name).x;
				if (w + 2 < width) {
					dl->AddText(ImVec2(xStart + 2, posY), 0xffffffff, name);
				}

				// Show tooltip
				if (ImGui::IsMouseHoveringRect(ra, rb)) 
				{
					const F32 t = F32((block.endTime - block.startTime) * 1000.0f);
					ImGui::BeginTooltip();
					ImGui::Text("%s (%.3f ms)", name, t);
					ImGui::EndTooltip();
				}
			};

			auto pos = FindStringChar(block.name, ':', 0);
			const String name = pos >= 0 ? block.name + pos + 2 : block.name;
			DrawBlock(block.startTime, block.endTime, name.c_str(), colors[blockIndex++ % ARRAYSIZE(colors)]);

			int childrenDepth = block.depth + 1;
			if (childrenDepth <= maxDepth)
			{
				while (++index < blocks.size())
				{
					int subDepth = blocks[index].depth;
					if (subDepth <= block.depth)
						break;

					if (subDepth == childrenDepth)
						OnTimelineBlockGUI(blocks[index], baseY, lineHeight, index, maxDepth, blocks, dl);
				}
			}		
		}

		void OnSelectedFrameChagned(I32 frame, bool isPaused)
		{
			if (isPaused && frame == selectedFrame)
				return;

			selectedFrame = frame;
			currentRange = GetBlockRange();

			// Init viewStart
			viewStart = currentRange.startTime;
			auto blockData = blocks.Get(selectedFrame);
			if (!blockData.empty())
			{
				F64 startTime = FLT_MAX;
				for (const auto& thread : blockData)
				{
					if (!thread.blocks.empty())
						startTime = std::min(startTime, thread.blocks[0].startTime);
				}

				if (startTime < FLT_MAX)
					viewStart = std::max(startTime, viewStart);
			}

		}

		void OnTimelineGUI(BlockRange range)
		{
			if (blocks.Count() == 0)
				return;

			F64 totalTime = updateTimesChart.GetSample(selectedFrame);
			auto blockData = blocks.Get(selectedFrame);
			if (blockData.empty())
				return;

			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->ChannelsSplit(2);

			const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
			fromX = ImGui::GetCursorScreenPos().x;
			fromY = ImGui::GetCursorScreenPos().y;
			toX = fromX + ImGui::GetContentRegionAvail().x;
			blockIndex = 0;

			for (const auto& thread : blockData)
			{
				if (thread.blocks.empty())
					continue;

				if (showMainThreadOnly && thread.name != "AsyncMainThread")
					continue;

				if (!ImGui::TreeNodeEx(thread.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s", thread.name.c_str()))
					continue;

				I32 maxDepth = 0;
				for (const auto& block : thread.blocks)
				{
					if (block.endTime <= 0.0f || range.SkipBlock(block))
						continue;

					maxDepth = std::max(maxDepth, block.depth);
				}

				I32 index = 0;
				float baseY = ImGui::GetCursorScreenPos().y;
				for (const auto& block : thread.blocks)
				{
					F64 time = std::max(block.endTime - block.startTime, 0.000000001);
					if (block.endTime <= 0.0f || range.SkipBlock(block))
						continue;
					
					if (block.depth == 0)
						OnTimelineBlockGUI(block, baseY, lineHeight, index, maxDepth, thread.blocks, dl);

					index++;
				}

				ImGui::Dummy(ImVec2(toX - fromX, (maxDepth + 1) * lineHeight));
				ImGui::TreePop();
			}

			if (ImGui::IsMouseHoveringRect(ImVec2(fromX, fromY), ImVec2(toX, ImGui::GetCursorScreenPos().y))) 
			{
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) 
					viewStart -= F64((ImGui::GetIO().MouseDelta.x / (toX - fromX)) * timelineRange);

				if (ImGui::GetIO().KeyCtrl) 
				{
					U32 range = (U32)(timelineRange * 10000);
					if (ImGui::GetIO().MouseWheel > 0 && range > 1) 
					{
						range >>= 1;
						timelineRange = range / 10000.0;
					}
					else if (ImGui::GetIO().MouseWheel < 0) 
					{
						range <<= 1;
						timelineRange = range / 10000.0;
					}
				}
			}
		}

		BlockRange GetBlockRange()
		{	
			BlockRange range;
			if (showLastUpdateBlocks)
			{
				auto blockData = blocks.Get(selectedFrame);
				if (!blockData.empty())
				{
					for (const auto& thread : blockData)
					{
						if (thread.blocks.empty())
							continue;

						for (const auto& block : thread.blocks)
						{
							if (block.depth == 0 && block.name == "Update")
							{					
								range.startTime = block.startTime;
								range.endTime = block.endTime;
								return range;
							}
						}
					}
				}
			}
		
			range.startTime = FLT_MIN;
			range.endTime = FLT_MAX;
			return range;
		}

		void OnGUI(bool isPaused) override
		{
			if (!ImGui::BeginTabItem("CPU"))
				return;

			updateTimesChart.OnGUI();
			if (blocks.Count() == 0)
			{
				ImGui::EndTabItem();
				return;
			}

			if (isPaused == true)
			{
				I32 selectedIndex = updateTimesChart.GetSelectedSampleIndex();
				if (selectedIndex != -1)
					OnSelectedFrameChagned(selectedIndex, true);
			}

			// Show timeline
			ImGui::Separator();
			OnTimelineGUI(currentRange);
			ImGui::NewLine();

			// Show table frame infos
			ImGui::Separator();	
			OnTableGUI(currentRange);

			ImGui::EndTabItem();
		}

		void Clear() override
		{
			updateTimesChart.Clear();
			blockNodes.clear();
			blocks.Clear();
		}

		void PrevFrame() override
		{
			I32 selectedIndex = updateTimesChart.GetSelectedSampleIndex();
			selectedIndex = std::max(selectedIndex - 1, 0);
			updateTimesChart.SetSelectedSampleIndex(selectedIndex);	
		}

		void NextFrame() override
		{
			I32 selectedIndex = updateTimesChart.GetSelectedSampleIndex();
			selectedIndex = std::min(selectedIndex + 1, MaxSamples);
			updateTimesChart.SetSelectedSampleIndex(selectedIndex);
		}
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	///  GPU profiler
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	struct GPUProfiler : ProfilerMode
	{
		void Update(ProfilerData& tools) override
		{
		}

		void OnGUI(bool isPaused) override
		{
			if (!ImGui::BeginTabItem("GPU"))
				return;

			ImGui::EndTabItem();
		}

		void Clear() override
		{
		}
	};

	class ProfilerWidgetImpl : public ProfilerWidget
	{
	private:
		bool isPaused = true;
		ProfilerTools& profilerTools;
		ProfilerData profilerData;
		Array<ProfilerMode*> profilerModes;
		bool showMainThreadOnly = false;
		bool lastUpdateOnly = true;

	public:
		ProfilerWidgetImpl(EditorApp& editor_) :
			editor(editor_),
			profilerTools(editor_.GetProfilerTools())
		{
			isOpen = false;

			profilerModes.push_back(CJING_NEW(OverallProfiler));
			profilerModes.push_back(CJING_NEW(CPUProfiler));
			profilerModes.push_back(CJING_NEW(GPUProfiler));
		}

		~ProfilerWidgetImpl()
		{
			for (auto mode : profilerModes)
				CJING_SAFE_DELETE(mode);
			profilerModes.clear();
		}

		void Update(F32 dt) override
		{
			PROFILE_FUNCTION();
			if (isPaused)
				return;

			// Get profiler data
			profilerData.cpuThreads = profilerTools.GetCPUThreads();
			profilerData.mainStats = profilerTools.GetMainStats();

			for (auto mode : profilerModes)
				mode->Update(profilerData);
		}

		void OnPause() 
		{
			ASSERT(isPaused == true);
		}

		void OnGUI() override
		{
			PROFILE_FUNCTION();
			if (!isOpen) 
				return;

			ImGuiWindowFlags flags = 
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoMove;;
			if (ImGui::Begin(ICON_FA_CHART_AREA "Profiler##profiler", &isOpen, flags))
			{
				// Start/Pause collect profile data
				if (ImGui::Button(isPaused ? ICON_FA_PLAY : ICON_FA_PAUSE))
				{
					isPaused = !isPaused;

					if (isPaused)
						OnPause();
				}

				ImGui::SameLine();
			
				// Clear all profile datas
				if (ImGui::Button(ICON_FA_UNDO_ALT))
				{
					for (auto mode : profilerModes)
						mode->Clear();
				}

				ImGui::SameLine();
				ImGuiEx::Rect(1.0f, ImGui::GetItemRectSize().y, 0xff3A3A3E);
				ImGui::SameLine();

				// Previous frame
				if (ImGui::Button(ICON_FA_ARROW_LEFT))
				{
					for (auto mode : profilerModes)
						mode->PrevFrame();
				}

				ImGui::SameLine();

				// Next frame
				if (ImGui::Button(ICON_FA_ARROW_RIGHT))
				{
					for (auto mode : profilerModes)
						mode->NextFrame();
				}

				ImGui::SameLine();
				ImGuiEx::Rect(1.0f, ImGui::GetItemRectSize().y, 0xff3A3A3E);
				ImGui::SameLine();

				// Show profiler advanced
				if (ImGui::Checkbox("MainThreadOnly", &showMainThreadOnly))
				{
					for (auto mode : profilerModes)
						mode->showMainThreadOnly = showMainThreadOnly;
				}
				ImGui::SameLine();
				if (ImGui::Checkbox("LastUpdateOnly", &lastUpdateOnly))
				{
					for (auto mode : profilerModes)
						mode->showLastUpdateBlocks = lastUpdateOnly;
				}

				// Show profiler tabs
				if (ImGui::BeginTabBar("tabs"))
				{
					for (auto& mode : profilerModes)
						mode->OnGUI(isPaused);
					
					ImGui::EndTabBar();
				}
			}
			ImGui::End();
		}

		const char* GetName()
		{
			return "ProfilerWidget";
		}

	private:
		EditorApp& editor;
	};

	UniquePtr<ProfilerWidget> ProfilerWidget::Create(EditorApp& app)
	{
		return CJING_MAKE_UNIQUE<ProfilerWidgetImpl>(app);
	}
}
}