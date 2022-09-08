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
		std::vector<ProfilerGPU::Block> gpuBlocks;
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
		String sample;
		SamplesBuffer<F32, ProfilerMode::MaxSamples> samples;
		int selectedSampleIndex = -1;

		const F32 HEIGHT = 60.0f;

	public:
		std::function<String(F32)> formatSample;

		SingleChart(const char* name_) :
			name(name_)
		{
		}

		void AddSample(F32 value)
		{
			samples.Add(value);
			sample = formatSample ? formatSample(value) : std::to_string(value);
		}

		F32 GetSample(I32 index)const {
			return samples.Get(index);
		}

		I32 GetSelectedSampleIndex()const {
			return selectedSampleIndex;
		}

		void SetSelectedSampleIndex(I32 index) 
		{
			if (selectedSampleIndex != index) 
			{
				F32 value = GetSample(index);
				sample = formatSample ? formatSample(value) : std::to_string(value);
			}
			selectedSampleIndex = index;
		}

		void OnGUI()
		{
			if (samples.Count() <= 0)
				return;

			// Draw title bg
			F32 bgWidth = ImGui::GetContentRegionAvail().x;
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 startPos = ImGui::GetCursorScreenPos();
			const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
			const ImVec2 rb(startPos.x + bgWidth, startPos.y + lineHeight);
			dl->AddRectFilled(startPos, rb, 0xff007ACC);

			// Draw title
			ImGuiEx::Label(name.c_str());
			ImGui::Text(sample.c_str());

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
					SetSelectedSampleIndex((I32)(x / innerWidth * samples.Count()));
				}
			}
		}

		void Clear()
		{
			samples.Clear();
			sample.clear();
		}
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	///  Overall profiler
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	struct OverallProfiler : ProfilerMode
	{
		SingleChart fpsChart;
		SingleChart updateTimesChart;
		SingleChart drawTimesChart;
		SingleChart cpuMemoryChart;
		SingleChart gpuMemoryChart;

		OverallProfiler() :
			fpsChart("FPS"),
			updateTimesChart("UpdateTimes"),
			drawTimesChart("DrawTimes(CPU)"),
			cpuMemoryChart("MemoryCPU"),
			gpuMemoryChart("MemoryGPU")
		{
			updateTimesChart.formatSample = [](F32 value)->String {
				return StaticString<32>().Sprintf("%.3f ms", value* 1000.0f).c_str();
			};
			drawTimesChart.formatSample = [](F32 value)->String {
				return StaticString<32>().Sprintf("%.3f ms", value * 1000.0f).c_str();
			};
			cpuMemoryChart.formatSample = [](F32 value)->String {
				return StaticString<32>().Sprintf("%d MB", (I32)value).c_str();
			};
			gpuMemoryChart.formatSample = [](F32 value)->String {
				return StaticString<32>().Sprintf("%d MB", (I32)value).c_str();
			};
		}

		void Update(ProfilerData& data) override
		{
			fpsChart.AddSample((F32)data.mainStats.fps);
			updateTimesChart.AddSample(data.mainStats.updateTimes);
			drawTimesChart.AddSample(data.mainStats.drawTimes);
			cpuMemoryChart.AddSample((F32)(data.mainStats.memoryCPU / 1024 / 1024));	// Bytes -> MB)
			gpuMemoryChart.AddSample((F32)(data.mainStats.memoryGPU / 1024 / 1024));	// Bytes -> MB
		}

		void OnGUI(bool isPaused) override
		{
			if (!ImGui::BeginTabItem("Overall"))
				return;

			fpsChart.OnGUI();
			updateTimesChart.OnGUI();
			drawTimesChart.OnGUI();
			cpuMemoryChart.OnGUI();
			gpuMemoryChart.OnGUI();

			ImGui::EndTabItem();
		}

		void Clear() override
		{
			fpsChart.Clear();
			updateTimesChart.Clear();
			drawTimesChart.Clear();
			cpuMemoryChart.Clear();
			gpuMemoryChart.Clear();
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

		static void DisplayNode(int index, int maxDepth, const Array<CPUBlockNode>& blocks)
		{
			const auto node = &blocks[index];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			bool hasDepth = false;
			int childrenDepth = node->depth + 1;
			if (childrenDepth <= maxDepth && index < blocks.size() - 1)
			{
				int subDepth = blocks[index + 1].depth;
				if (subDepth == childrenDepth)
					hasDepth = true;
			}

			ImGuiTreeNodeFlags flag = hasDepth ?
				ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen :
				ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;

			bool open = ImGui::TreeNodeEx(node->name.c_str(), flag);
			ImGui::TableNextColumn();
			ImGui::Text("%.2f%%", node->total);
			ImGui::TableNextColumn();
			ImGui::Text("%.4f", node->time);

			if (hasDepth && open)
			{
				while (++index < blocks.size())
				{
					int subDepth = blocks[index].depth;
					if (subDepth <= node->depth)
						break;

					if (subDepth == childrenDepth)
						DisplayNode(index, maxDepth, blocks);
				}
				ImGui::TreePop();
			}
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
			updateTimesChart.formatSample = [](F32 value)->String {
				return StaticString<32>().Sprintf("%.3f ms", value * 1000.0f).c_str();
			};
		}

		void Update(ProfilerData& data) override
		{
			updateTimesChart.AddSample(data.mainStats.deltaTimes);
			blocks.Add(data.cpuThreads);

			OnSelectedFrameChagned(0, false);
		}
		
		void OnTableGUI(BlockRange range)
		{
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

				blockNodes.clear();

				I32 maxDepth = 0;
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

					maxDepth = std::max(maxDepth, block.depth);
				}

				static ImGuiTableFlags flags =
					ImGuiTableFlags_BordersV |
					ImGuiTableFlags_BordersOuterH |
					ImGuiTableFlags_Resizable |
					ImGuiTableFlags_RowBg |
					ImGuiTableFlags_NoBordersInBody;

				if (ImGui::BeginTable(thread.name.c_str(), 3, flags))
				{
					const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
					ImGui::TableSetupColumn("Block", ImGuiTableColumnFlags_NoHide);
					ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
					ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 18.0f);
					ImGui::TableHeadersRow();

					for (int i = 0; i < blockNodes.size(); i++)
					{
						if (blockNodes[i].depth == 0)
							CPUBlockNode::DisplayNode(i, maxDepth, blockNodes);
					}

					ImGui::EndTable();
				}
			}
		}

		F64 viewStart = 0.0f;
		F32 fromX = 0.0f;
		F32 fromY = 0.0f;
		F32 toX = 0.0f;
		void OnTimelineBlockGUI(F32 baseY, F32 lineHeight, I32 index, I32 maxDepth, const std::vector<Profiler::Block>& blocks, ImDrawList* dl)
		{
			const auto& block = blocks[index];
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
			U32 color;
			if (block.type == Profiler::BlockType::FIBER)
			{
				static const U32 WaitBlockColor = 0xff0000ff;
				color = WaitBlockColor;
			}
			else
			{
				U32 hash = (U32)StringID(name.c_str()).GetHashValue();
				color = colors[hash % ARRAYSIZE(colors)];
			}

			DrawBlock(block.startTime, block.endTime, name.c_str(), color);

			int childrenDepth = block.depth + 1;
			if (childrenDepth <= maxDepth)
			{
				while (++index < blocks.size())
				{
					int subDepth = blocks[index].depth;
					if (subDepth <= block.depth)
						break;

					if (subDepth == childrenDepth)
						OnTimelineBlockGUI(baseY, lineHeight, index, maxDepth, blocks, dl);
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
						OnTimelineBlockGUI(baseY, lineHeight, index, maxDepth, thread.blocks, dl);

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
			selectedIndex = std::min(selectedIndex + 1, MaxSamples - 1);
			updateTimesChart.SetSelectedSampleIndex(selectedIndex);
		}
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	///  GPU profiler
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	struct GPUProfiler : ProfilerMode
	{
	public:
		U32 selectedFrame = 0;
		F64 timelineRange = 16.0f;
		bool timeRangeInit = false;

		SingleChart drawTimesCPUChart;
		SingleChart drawTimesGPUChart;
		SamplesBuffer<std::vector<ProfilerGPU::Block>, ProfilerMode::MaxSamples> blocks;

		const U32 colors[6] = {
			0xFF44355B,
			0xFF51325C,
			0xFF1355B5,
			0xFF4365B2,
			0xFF156621,
			0xFF72A268,
		};

	public:
		GPUProfiler() :
			drawTimesCPUChart("DrawTime(CPU)"),
			drawTimesGPUChart("DrawTime(GPU)")
		{
			drawTimesCPUChart.formatSample = [](F32 value)->String {
				return StaticString<32>().Sprintf("%.3f ms", value * 1000.0f).c_str();
			};
			drawTimesGPUChart.formatSample = [](F32 value)->String {
				return StaticString<32>().Sprintf("%.3f ms", value).c_str();
			};
		}

		void Update(ProfilerData& data) override
		{
			drawTimesCPUChart.AddSample(data.mainStats.drawTimes);
			drawTimesGPUChart.AddSample(data.mainStats.drawTimesGPU);
			blocks.Add(data.gpuBlocks);
		}

		void OnGUI(bool isPaused) override
		{
			if (!ImGui::BeginTabItem("GPU"))
				return;
			
			drawTimesCPUChart.OnGUI();
			drawTimesGPUChart.OnGUI();

			if (isPaused == false)
			{
				viewStart = 0.0f;
				timeRangeInit = false;
			}
			else
			{
				I32 selectedIndex = drawTimesCPUChart.GetSelectedSampleIndex();
				if (selectedIndex != -1)
					OnSelectedFrameChagned(selectedIndex, true);

				drawTimesGPUChart.SetSelectedSampleIndex(selectedIndex);
			}

			// Show timeline
			ImGui::Separator();
			OnTimelineGUI();
			ImGui::NewLine();

			ImGui::Separator();
			OnTableGUI();

			ImGui::EndTabItem();
		}

	public:
		F32 lineHeight = 0.0f;
		F64 viewStart = 0.0f;
		F32 fromX = 0.0f;
		F32 fromY = 0.0f;
		F32 toX = 0.0f;
		F32 OnTimelineBlockGUI(I32 index, I32 maxDepth, const std::vector<ProfilerGPU::Block>& blocks, ImDrawList* dl)
		{
			auto& block = blocks[index];
			auto DrawBlock = [&](F64 from, F64 to, const char* name, U32 color)
			{
				if (to < viewStart)
					return;

				const F32 tStart = F32((from - viewStart) / timelineRange);
				const F32 tEnd = F32((to - viewStart) / timelineRange);
				const F32 xStart = fromX * (1 - tStart) + toX * tStart;
				const F32 xEnd = fromX * (1 - tEnd) + toX * tEnd;

				// Block background
				F32 posY = fromY + block.depth * lineHeight;
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
					ImGui::BeginTooltip();
					ImGui::Text("%s (%.3f ms)", name, block.time);
					ImGui::EndTooltip();
				}
			};

			U32 hash = (U32)StringID(block.name).GetHashValue();
			auto color = colors[hash % ARRAYSIZE(colors)];
			DrawBlock(block.startTime, block.startTime + block.time, block.name, color);

			int childrenDepth = block.depth + 1;
			if (childrenDepth <= maxDepth)
			{
				// Count sub blocks total duration
				double subBlocksDuration = 0;
				int tmpIndex = index;
				while (++tmpIndex < blocks.size())
				{
					int subDepth = blocks[tmpIndex].depth;
					if (subDepth <= block.depth)
						break;

					if (subDepth == childrenDepth)
						subBlocksDuration += blocks[tmpIndex].time;
				}

				// Skip if has no sub events
				if (subBlocksDuration > 0)
				{
					while (++index < blocks.size())
					{
						int subDepth = blocks[index].depth;
						if (subDepth <= block.depth)
							break;

						if (subDepth == childrenDepth)
							OnTimelineBlockGUI(index, maxDepth, blocks, dl);
					}
				}
			}

			return block.time;
		}

		void OnTimelineGUI()
		{
			if (blocks.Count() == 0)
				return;

			auto blockData = blocks.Get(selectedFrame);
			if (blockData.empty())
				return;

			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->ChannelsSplit(2);

			float baseY = ImGui::GetCursorScreenPos().y;
			lineHeight = ImGui::GetTextLineHeightWithSpacing();
			fromX = ImGui::GetCursorScreenPos().x;
			fromY = ImGui::GetCursorScreenPos().y;
			toX = fromX + ImGui::GetContentRegionAvail().x;

			I32 maxDepth = 0;
			for (const auto& block : blockData)
				maxDepth = std::max(maxDepth, block.depth);

			if (!timeRangeInit)
			{
				timelineRange = blockData[0].time;
				viewStart = blockData[0].startTime;
				timeRangeInit = true;
			}

			for (I32 index = 0; index < blockData.size(); index++)
			{
				if (blockData[index].depth == 0)
					OnTimelineBlockGUI(index, maxDepth, blockData, dl);
			}
			ImGui::Dummy(ImVec2(toX - fromX, (maxDepth + 1) * lineHeight));

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

	public:
		static void OnTableRowGUI(int index, int maxDepth, F32 totalTimes, const std::vector<ProfilerGPU::Block>& blocks)
		{
			const auto& block = blocks[index];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			bool hasDepth = false;
			int childrenDepth = block.depth + 1;
			if (childrenDepth <= maxDepth && index < blocks.size() - 1)
			{
				int subDepth = blocks[index + 1].depth;
				if (subDepth == childrenDepth)
					hasDepth = true;
			}

			ImGuiTreeNodeFlags flag = hasDepth ?
				ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen :
				ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;

			bool open = ImGui::TreeNodeEx(block.name.c_str(), flag);
			ImGui::TableNextColumn();
			ImGui::Text("%.2f%%", (int)(block.time / totalTimes * 1000.0f) / 10.0f);
			ImGui::TableNextColumn();
			ImGui::Text("%.4f", (block.time * 10000.0f) / 10000.0f);
			ImGui::TableNextColumn();
			ImGui::Text("%d", block.stats.drawCalls);
			ImGui::TableNextColumn();
			ImGui::Text("%d", block.stats.vertices);
			ImGui::TableNextColumn();
			ImGui::Text("%d", block.stats.triangles);

			if (hasDepth && open)
			{
				while (++index < blocks.size())
				{
					int subDepth = blocks[index].depth;
					if (subDepth <= block.depth)
						break;

					if (subDepth == childrenDepth)
						OnTableRowGUI(index, maxDepth, totalTimes, blocks);
				}
				ImGui::TreePop();
			}
		}

		void OnTableGUI()
		{
			if (blocks.Count() == 0)
				return;

			auto blockData = blocks.Get(selectedFrame);
			if (blockData.empty())
				return;

			I32 maxDepth = 0;
			for (const auto& block : blockData)
				maxDepth = std::max(maxDepth, block.depth);

			F32 totalDrawTime = blockData[0].time;

			static ImGuiTableFlags flags =
				ImGuiTableFlags_BordersV |
				ImGuiTableFlags_BordersOuterH |
				ImGuiTableFlags_Resizable |
				ImGuiTableFlags_RowBg |
				ImGuiTableFlags_NoBordersInBody;

			if (ImGui::BeginTable("GPUBlocks", 6, flags))
			{
				const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
				ImGui::TableSetupColumn("Block", ImGuiTableColumnFlags_NoHide);
				ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
				ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 18.0f);
				ImGui::TableSetupColumn("Draw Calls", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
				ImGui::TableSetupColumn("Vertices", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
				ImGui::TableSetupColumn("Triangles", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
				ImGui::TableHeadersRow();

				for (int i = 0; i < blockData.size(); i++)
				{
					if (blockData[i].depth == 0)
						OnTableRowGUI(i, maxDepth, totalDrawTime, blockData);
				}

				ImGui::EndTable();
			}
		}

		void OnSelectedFrameChagned(I32 frame, bool isPaused)
		{
			if (isPaused && frame == selectedFrame)
				return;

			selectedFrame = frame;
		}

		void Clear() override
		{
			drawTimesCPUChart.Clear();
			drawTimesGPUChart.Clear();
			blocks.Clear();
		}

		void PrevFrame() override
		{
			I32 selectedIndex = drawTimesCPUChart.GetSelectedSampleIndex();
			selectedIndex = std::max(selectedIndex - 1, 0);
			drawTimesCPUChart.SetSelectedSampleIndex(selectedIndex);
		}

		void NextFrame() override
		{
			I32 selectedIndex = drawTimesCPUChart.GetSelectedSampleIndex();
			selectedIndex = std::min(selectedIndex + 1, MaxSamples - 1);
			drawTimesCPUChart.SetSelectedSampleIndex(selectedIndex);
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
			profilerData.gpuBlocks = profilerTools.GetGPUBlocks();

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