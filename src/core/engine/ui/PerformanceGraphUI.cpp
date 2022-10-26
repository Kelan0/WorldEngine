#include "core/engine/ui/PerformanceGraphUI.h"
#include "core/application/Application.h"
#include "core/util/Profiler.h"
#include "core/util/Util.h"
#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_internal.h"
#include "extern/imgui/imgui_stdlib.h"

PerformanceGraphUI::PerformanceGraphUI():
        m_profilingPaused(false),
        m_graphsNormalized(false),
        m_clearFrames(true),
        m_showFrameTime90Percentile(false),
        m_showFrameTime99Percentile(false),
        m_showFrameTime999Percentile(false),
        m_graphVisibilityMode(GraphVisibilityMode_Both),
        m_profilerDisplayMode(ProfilerDisplayMode_HotFunctionList),
        m_maxFrameProfiles(500),
        m_averageAccumulationDuration(std::chrono::milliseconds(333)),
        m_lastAverageAccumulationTime(Performance::zero_moment),
        m_averageAccumulationFrameCount(0),
        m_rollingAverageUpdateFactor(0.05F) {
}

PerformanceGraphUI::~PerformanceGraphUI() = default;


void PerformanceGraphUI::update(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::update")

    if (m_clearFrames) {
        m_clearFrames = false;
        for (auto& [threadId, allProfiles] : m_threadFrameProfileData) {
            allProfiles.clear();
        }

        m_gpuFrameProfileData.clear();

        for (auto& [threadId, frameGraphInfo] : m_threadFrameGraphInfo) {
            frameGraphInfo.frameTimes.clear();
            frameGraphInfo.sortedFrameTimes.clear();
            frameGraphInfo.allFramesTimeAvg = 0.0F;
            frameGraphInfo.frameTimeAvg = 0.0F;
            frameGraphInfo.frameTimeSum = 0.0F;
            frameGraphInfo.frameTimePercentile90 = 0.0F;
            frameGraphInfo.frameTimePercentile99 = 0.0F;
            frameGraphInfo.frameTimePercentile999 = 0.0F;
            frameGraphInfo.frameSequence = 0;
        }

        m_gpuFrameGraphInfo.frameTimes.clear();
        m_gpuFrameGraphInfo.sortedFrameTimes.clear();
        m_gpuFrameGraphInfo.allFramesTimeAvg = 0.0F;
        m_gpuFrameGraphInfo.frameTimeAvg = 0.0F;
        m_gpuFrameGraphInfo.frameTimeSum = 0.0F;
        m_gpuFrameGraphInfo.frameTimePercentile90 = 0.0F;
        m_gpuFrameGraphInfo.frameTimePercentile99 = 0.0F;
        m_gpuFrameGraphInfo.frameTimePercentile999 = 0.0F;
        m_gpuFrameGraphInfo.frameSequence = 0;

//        m_averageAccumulationFrameCount = 0;
    }

    if (m_profilingPaused) {
        return;
    }

    PROFILE_REGION("PerformanceGraphUI::update - Get thread profile data")
    for (auto& [threadId, threadProfiles] : m_currentThreadProfiles) {
        threadProfiles.clear(); // Keeps the array allocated for the next frame
    }

    Profiler::getFrameProfile(m_currentThreadProfiles);

    uint64_t mainThreadId = Application::instance()->getHashedMainThreadId();

    std::vector<uint32_t> layerPath;
    size_t parentIndex = SIZE_MAX;
    size_t popCount = 0;

    const float rollingAvgAlpha = 0.05F;
    for (ProfileLayer& layer : m_layers) {
//        layer.accumulatedSelfTimeAvg = glm::lerp(layer.accumulatedSelfTimeAvg, layer.accumulatedSelfTime, rollingAvgAlpha);
//        layer.accumulatedTotalTimeAvg = glm::lerp(layer.accumulatedTotalTimeAvg, layer.accumulatedTotalTime, rollingAvgAlpha);
//        if (layer.accumulatedSelfTimeAvg == 0.0F) layer.accumulatedSelfTimeAvg = layer.accumulatedSelfTime;
//        if (layer.accumulatedTotalTimeAvg == 0.0F) layer.accumulatedTotalTimeAvg = layer.accumulatedTotalTime;

        layer.accumulatedSelfTimeSum += layer.accumulatedSelfTime;
        layer.accumulatedTotalTimeSum += layer.accumulatedTotalTime;
        layer.accumulatedSelfTime = 0.0F;
        layer.accumulatedTotalTime = 0.0F;
    }

    PROFILE_REGION("PerformanceGraphUI::update - Process thread profile data")
    for (const auto& [threadId, threadProfiles] : m_currentThreadProfiles) {
        FrameGraphInfo& threadInfo = m_threadFrameGraphInfo[threadId];
        std::vector<FrameProfileData>& allProfiles = m_threadFrameProfileData[threadId];


        FrameProfileData* previousFrame = allProfiles.empty() ? nullptr : &allProfiles.back();
        FrameProfileData* currentFrame = &allProfiles.emplace_back();

        initializeProfileDataTree(currentFrame, threadProfiles.data(), threadProfiles.size(), sizeof(ThreadProfile));

        initializeProfileTreeLayers(currentFrame, threadProfiles.data(), threadProfiles.size(), sizeof(ThreadProfile), layerPath);

        checkFrameProfileTreeStructureChanged(currentFrame, previousFrame);

        for (size_t i = 0; i < threadProfiles.size(); ++i) {
            const ThreadProfile& profile = threadProfiles[i];
            currentFrame->profileData[i].elapsedMillis = (float)Performance::milliseconds(profile.startTime, profile.endTime);
        }

        if (threadId == mainThreadId) {
            for (size_t i = 0; i < currentFrame->profileData.size(); ++i) {
                const ProfileData& profile = currentFrame->profileData[i];
                ProfileLayer& uniqueLayer = m_layers[profile.layerIndex];
                uniqueLayer.accumulatedTotalTime += profile.elapsedMillis;
                uniqueLayer.accumulatedSelfTime += profile.elapsedMillis;

//                ProfileLayer& pathLayer = m_layers[profile.pathIndex];
//                pathLayer.accumulatedTotalTime += profile.elapsedMillis;
//                pathLayer.accumulatedSelfTime += profile.elapsedMillis;

                if (profile.parentOffset != 0) {
                    ProfileData& parentProfile = currentFrame->profileData[i - profile.parentOffset];
                    ProfileLayer& parentUniqueLayer = m_layers[parentProfile.layerIndex];
                    parentUniqueLayer.accumulatedSelfTime -= profile.elapsedMillis;

//                    ProfileLayer& parentPathLayer = m_layers[parentProfile.pathIndex];
//                    parentPathLayer.accumulatedSelfTime -= profile.elapsedMillis;
                }
            }
        }

        float rootElapsed = (float)Performance::milliseconds(threadProfiles[0].startTime, threadProfiles[0].endTime);
        updateFrameGraphInfo(threadInfo, rootElapsed);
    }

    PROFILE_REGION("PerformanceGraphUI::update - Get GPU profile data")
    m_currentGpuProfiles.clear();

    if (Profiler::getLatestGpuFrameProfile(m_currentGpuProfiles)) {
        PROFILE_REGION("Process GPU profile data")
        FrameProfileData* previousFrame = m_gpuFrameProfileData.empty() ? nullptr : &m_gpuFrameProfileData.back();
        FrameProfileData* currentFrame = &m_gpuFrameProfileData.emplace_back();

        initializeProfileDataTree(currentFrame, m_currentGpuProfiles.data(), m_currentGpuProfiles.size(), sizeof(GPUProfile));

        initializeProfileTreeLayers(currentFrame, m_currentGpuProfiles.data(), m_currentGpuProfiles.size(), sizeof(GPUProfile), layerPath);

        checkFrameProfileTreeStructureChanged(currentFrame, previousFrame);

        for (size_t i = 0; i < m_currentGpuProfiles.size(); ++i) {
            const GPUProfile& profile = m_currentGpuProfiles[i];
            currentFrame->profileData[i].elapsedMillis = (float)(profile.endQuery.time - profile.startQuery.time);
        }


        float rootElapsed = (float)(m_currentGpuProfiles[0].endQuery.time - m_currentGpuProfiles[0].startQuery.time);
        updateFrameGraphInfo(m_gpuFrameGraphInfo, rootElapsed);
    }
    PROFILE_END_REGION()

    updateAccumulatedAverages();

    flushOldFrames();
}

void PerformanceGraphUI::draw(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::draw")

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;

    if (!ImGui::Begin("Profiler")) {
        ImGui::End(); // Early exit - Window is not visible.
        return;
    }
    drawHeaderBar();
    ImGui::Separator();
    drawProfileContent(dt);
    ImGui::End();
}

void PerformanceGraphUI::drawHeaderBar() {
    PROFILE_SCOPE("PerformanceGraphUI::drawHeaderBar")

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;

    float itemWidth = 100.0F;

    ImGui::BeginGroup();
    {
        ImGui::Checkbox("Pause", &m_profilingPaused);

        ImGui::SameLine(0.0F, 10.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0F, 10.0F);

        ImGui::Checkbox("Normalize", &m_graphsNormalized);

        ImGui::SameLine(0.0F, 10.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0F, 10.0F);

        size_t filterTextCapacity = m_profileNameFilterText.size() + 256;
        if (filterTextCapacity >= m_profileNameFilterText.capacity())
            m_profileNameFilterText.reserve(filterTextCapacity + filterTextCapacity / 2);

        itemWidth = 250.0F;
        ImGui::PushItemWidth(itemWidth);
        if (ImGui::InputText("Filter", &m_profileNameFilterText, ImGuiInputTextFlags_None)) {
            m_profileNameFilterSearchTerms.clear();
             Util::splitString(m_profileNameFilterText, ' ', m_profileNameFilterSearchTerms);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine(0.0F, 10.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0F, 10.0F);

        itemWidth = 100.0F;
        static const char* GRAPH_VISIBILITY_OPTIONS[] = {"Show Both Graphs", "Show CPU Graph", "Show GPU Graph"};
        for (const char* option : GRAPH_VISIBILITY_OPTIONS) itemWidth = glm::max(itemWidth, ImGui::CalcTextSize(option).x);
        ImGui::PushItemWidth(itemWidth + 30.0F);
        if (ImGui::BeginCombo("##WhichGraph", GRAPH_VISIBILITY_OPTIONS[m_graphVisibilityMode], ImGuiComboFlags_PopupAlignLeft)) {
            if (ImGui::Selectable(GRAPH_VISIBILITY_OPTIONS[GraphVisibilityMode_Both], m_graphVisibilityMode == GraphVisibilityMode_Both)) m_graphVisibilityMode = GraphVisibilityMode_Both;
            if (ImGui::Selectable(GRAPH_VISIBILITY_OPTIONS[GraphVisibilityMode_CPU], m_graphVisibilityMode == GraphVisibilityMode_CPU)) m_graphVisibilityMode = GraphVisibilityMode_CPU;
            if (ImGui::Selectable(GRAPH_VISIBILITY_OPTIONS[GraphVisibilityMode_GPU], m_graphVisibilityMode == GraphVisibilityMode_GPU)) m_graphVisibilityMode = GraphVisibilityMode_GPU;
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::SameLine(0.0F, 20.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0F, 10.0F);

        itemWidth = 100.0F;
        static const char* PROFILE_DISPLAY_MODE_OPTIONS[] = {"Show Call Stack", "Show Hot Functions"};
        for (const char* option : PROFILE_DISPLAY_MODE_OPTIONS) itemWidth = glm::max(itemWidth, ImGui::CalcTextSize(option).x);
        ImGui::PushItemWidth(itemWidth + 30.0F);
        if (ImGui::BeginCombo("##WhichDisplayMode", PROFILE_DISPLAY_MODE_OPTIONS[m_profilerDisplayMode], ImGuiComboFlags_PopupAlignLeft)) {
            if (ImGui::Selectable(PROFILE_DISPLAY_MODE_OPTIONS[ProfilerDisplayMode_CallStack], m_profilerDisplayMode == ProfilerDisplayMode_CallStack)) m_profilerDisplayMode = ProfilerDisplayMode_CallStack;
            if (ImGui::Selectable(PROFILE_DISPLAY_MODE_OPTIONS[ProfilerDisplayMode_HotFunctionList], m_profilerDisplayMode == ProfilerDisplayMode_HotFunctionList)) m_profilerDisplayMode = ProfilerDisplayMode_HotFunctionList;
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::SameLine(0.0F, 20.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0F, 10.0F);

        if (ImGui::Button("Clear Frames"))
            m_clearFrames = true;

        ImGui::SameLine(0.0F, 10.0F);

        //ImGui::Text("%llu frames, %llu profiles across %llu threads\n", m_frameProfiles.size(), frameProfile.numProfiles, frameProfile.threadProfiles.size());
    }
    ImGui::EndGroup();
}

void PerformanceGraphUI::drawProfileContent(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::drawProfileContent")

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;

    ImGui::BeginGroup();
    {
        ImGui::BeginColumns("frameGraph-profileTree", 2);
        {
            if (m_profilerDisplayMode == ProfilerDisplayMode_CallStack) {
                drawProfileCallStackTree(dt);
            } else if (m_profilerDisplayMode == ProfilerDisplayMode_HotFunctionList) {
                drawProfileHotFunctionList(dt);
            }

            ImGui::NextColumn();

            drawFrameGraphs(dt);
        }
        ImGui::EndColumns();
    }
    ImGui::EndGroup();
}

void PerformanceGraphUI::drawProfileCallStackTree(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::drawProfileCallStackTree")

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;


    if (ImGui::BeginChild("ProfileTree")) {
        int count = 5;
        ImGui::BeginColumns("ProfileTreeColumns", count, ImGuiColumnsFlags_NoPreserveWidths);
        {
            // TODO: this should only be initialized once
            float x = ImGui::GetWindowContentRegionMax().x;
            ImGui::SetColumnOffset(count--, x);
            x -= 60;
            ImGui::SetColumnOffset(count--, x);
            x -= 60;
//            ImGui::SetColumnOffset(count--, x);
//            x -= 80;
//            ImGui::SetColumnOffset(count--, x);
//            x -= 100;
            ImGui::SetColumnOffset(count--, x);
            x -= 80;
            ImGui::SetColumnOffset(count--, x);
            x -= 100;
            ImGui::SetColumnOffset(count--, x);

            if (x >= 0) {
                ImGui::Text("Profile Name");
                ImGui::NextColumn();
                ImGui::Text("CPU Time");
                ImGui::NextColumn();
                ImGui::Text("CPU %%");
//                ImGui::NextColumn();
//                ImGui::Text("GPU Time");
//                ImGui::NextColumn();
//                ImGui::Text("GPU %%");
                ImGui::NextColumn();
                ImGui::Text("Colour");
                ImGui::NextColumn();
                ImGui::Text("Focus");
                ImGui::NextColumn();

                uint64_t threadId = Application::instance()->getHashedMainThreadId();
                auto it = m_threadFrameProfileData.find(threadId);
                if (it != m_threadFrameProfileData.end() && !it->second.empty()) {
                    const FrameProfileData& currentFrame = it->second.back();
                    std::vector<size_t> temp;
                    auto it1 = m_threadFrameGraphInfo.find(threadId);
                    FrameGraphInfo& frameGraphInfo = it1->second;

                    PROFILE_REGION("PerformanceGraphUI::buildProfileTree")

                    if (!m_profileNameFilterSearchTerms.empty()) {
                        updateFilteredLayerPaths(currentFrame.profileData);
                    }
                    buildProfileTree(currentFrame, frameGraphInfo, 0, ProfileTreeSortOrder_Default, temp);
                    PROFILE_END_REGION()
                }
            }
        }
        ImGui::EndColumns();
    }
    ImGui::EndChild();
}

void PerformanceGraphUI::drawProfileHotFunctionList(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::drawProfileHotFunctionList")

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;

    if (ImGui::BeginChild("UniqueLayerList")) {
        int count = 4;
        if (ImGui::BeginTable("UniqueLayerListTable", count, ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Profile Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("CPU Time", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 80);
            ImGui::TableSetupColumn("CPU %%", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 60);
            ImGui::TableSetupColumn("Colour", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 100);
            ImGui::TableHeadersRow();

            uint64_t threadId = Application::instance()->getHashedMainThreadId();
            auto it = m_threadFrameProfileData.find(threadId);
            if (it != m_threadFrameProfileData.end() && !it->second.empty()) {
                const FrameProfileData& currentFrame = it->second.back();
                const ProfileData& rootProfile = currentFrame.profileData[0];
                const ProfileLayer& rootProfileLayer = m_layers[rootProfile.layerIndex];

                PROFILE_REGION("PerformanceGraphUI::drawProfileHotFunctionList - Allocate layer indices")
                std::vector<size_t> uniqueLayerIndices;
                uniqueLayerIndices.emplace_back(rootProfile.layerIndex);

                uniqueLayerIndices.reserve(m_uniqueLayerIndexMap.size() + 1);
                for (const auto& [layerName, layerIndex] : m_uniqueLayerIndexMap) {
                    if (!m_profileNameFilterSearchTerms.empty() && !matchSearchTerms(m_layers[layerIndex].layerName, m_profileNameFilterSearchTerms))
                        continue;
                    uniqueLayerIndices.emplace_back(layerIndex);
                }


                PROFILE_REGION("PerformanceGraphUI::drawProfileHotFunctionList - Sort layers")
                ImGuiTableSortSpecs* tableSortSpecs = ImGui::TableGetSortSpecs();
                if (tableSortSpecs != nullptr && tableSortSpecs->SpecsCount > 0) {
//                    if (tableSortSpecs->Specs[0].ColumnIndex == 0) { // Sort by name
//                        std::sort(uniqueLayerIndices.begin(), uniqueLayerIndices.end(), [&tableSortSpecs, this](const uint32_t& lhs, const uint32_t& rhs) {
//                            return m_layers[lhs].layerName < m_layers[rhs].layerName;
//                        });
//                    }
                    if (tableSortSpecs->Specs[0].ColumnIndex == 1) { // Sort by CPU time
                        if (tableSortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Ascending) {
                            std::sort(uniqueLayerIndices.begin() + 1, uniqueLayerIndices.end(), [this](const size_t& lhs, const size_t& rhs) {
                                return m_layers[lhs].accumulatedSelfTimeAvg < m_layers[rhs].accumulatedSelfTimeAvg;
                            });
                        } else if (tableSortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Descending) {
                            std::sort(uniqueLayerIndices.begin() + 1, uniqueLayerIndices.end(), [this](const size_t& lhs, const size_t& rhs) {
                                return m_layers[lhs].accumulatedSelfTimeAvg > m_layers[rhs].accumulatedSelfTimeAvg;
                            });
                        }
                    }
                }
                PROFILE_REGION("PerformanceGraphUI::drawProfileHotFunctionList - Display layers")

                float s = 1.0F / 255.0F;
                ImVec4 tempColour;
                tempColour.w = 1.0F;

                std::string id = std::string("##");
                id.reserve(256); // We avoid string re-allocations

                float invRootPercentDivisor = 1.0F / (rootProfileLayer.accumulatedTotalTimeAvg * 0.01F);

                for (size_t i = 0; i < uniqueLayerIndices.size(); ++i) {
                    const size_t& layerIndex = uniqueLayerIndices[i];
                    ProfileLayer& uniqueLayer = m_layers[layerIndex];

                    // The first layer is always for the root level, so we want to show the total frame time here.
                    float layerTime = i == 0 ? uniqueLayer.accumulatedTotalTimeAvg : uniqueLayer.accumulatedSelfTimeAvg;

                    id.reserve(uniqueLayer.layerName.size() + 2);
                    uniqueLayer.layerName.copy(&id[2], uniqueLayer.layerName.size(), 0);

//                    tempColour = ImGui::ColorConvertU32ToFloat4(uniqueLayer.colour); // This is oddly slow
                    tempColour.x = (float)((uniqueLayer.colour >> IM_COL32_R_SHIFT) & 0xFF) * s;
                    tempColour.y = (float)((uniqueLayer.colour >> IM_COL32_G_SHIFT) & 0xFF) * s;
                    tempColour.z = (float)((uniqueLayer.colour >> IM_COL32_B_SHIFT) & 0xFF) * s;

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", uniqueLayer.layerName.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f msec", layerTime);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f %%", layerTime * invRootPercentDivisor);
                    ImGui::TableNextColumn();
                    if (ImGui::ColorEdit3(id.c_str(), &tempColour.x, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs))
                        uniqueLayer.colour = ImGui::ColorConvertFloat4ToU32(tempColour);
                }
                PROFILE_END_REGION()
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

void PerformanceGraphUI::drawFrameGraphs(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::drawFrameGraphs")

    if (!ImGui::BeginChild("FrameGraphContainer")) {
        ImGui::EndChild();
        return;
    }
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    uint64_t threadId = Application::instance()->getHashedMainThreadId();


    const float padding = 2.0F;

    float x = window->Pos.x;
    float y = window->Pos.y;
    float w = window->Size.x;
    float h = window->Size.y;

    if (m_graphVisibilityMode == GraphVisibilityMode_Both) {
        h = h / 2 - padding;
    }

    if (m_graphVisibilityMode == GraphVisibilityMode_Both || m_graphVisibilityMode == GraphVisibilityMode_CPU) {
        auto it0 = m_threadFrameProfileData.find(threadId);
        auto it1 = m_threadFrameGraphInfo.find(threadId);
        if (it0 != m_threadFrameProfileData.end() && it1 != m_threadFrameGraphInfo.end()) {
            const std::vector<FrameProfileData>& mainThreadFrameData = it0->second;
            FrameGraphInfo& mainThreadFrameGraphInfo = it1->second;
            drawFrameGraph(dt, "CPU_FrameGraph", mainThreadFrameData, mainThreadFrameGraphInfo, x, y, w, h, padding);

            if (m_gpuFrameGraphInfo.heightScaleMsec < mainThreadFrameGraphInfo.heightScaleMsec) {
                m_gpuFrameGraphInfo.heightScaleMsec = mainThreadFrameGraphInfo.heightScaleMsec;
            } else if (mainThreadFrameGraphInfo.heightScaleMsec < m_gpuFrameGraphInfo.heightScaleMsec) {
                mainThreadFrameGraphInfo.heightScaleMsec = m_gpuFrameGraphInfo.heightScaleMsec;
            }
        }
    }

    if (m_graphVisibilityMode == GraphVisibilityMode_Both) {
        y += h + padding;
    }

    if (m_graphVisibilityMode == GraphVisibilityMode_Both || m_graphVisibilityMode == GraphVisibilityMode_GPU) {
        drawFrameGraph(dt, "GPU_FrameGraph", m_gpuFrameProfileData, m_gpuFrameGraphInfo, x, y, w, h, padding);
    }

    ImGui::EndChild();
}

void PerformanceGraphUI::drawFrameGraph(const double& dt, const char* strId, const std::vector<FrameProfileData>& frameData, FrameGraphInfo& frameGraphInfo, const float& x, const float& y, const float& w, const float& h, const float& padding) {
    PROFILE_SCOPE("PerformanceGraphUI::drawFrameGraph")

    if (frameData.empty())
        return;

    const float margin = 0.0F;
    if (w < (padding + margin) * 2 || h < (padding + margin) * 2)
        return;

    ImVec2 bbmin = ImVec2(x, y);
    ImVec2 bbmax = ImVec2(x + w, y + h);
    ImVec2 bbminInner = ImVec2(bbmin.x + padding, bbmin.y + padding);
    ImVec2 bbmaxInner = ImVec2(bbmax.x - padding, bbmax.y - padding);
    ImVec2 bbminOuter = ImVec2(bbmin.x - margin, bbmin.y - margin);
    ImVec2 bbmaxOuter = ImVec2(bbmax.x + margin, bbmax.y + margin);

    if (!ImGui::BeginChild(strId, ImVec2(bbmaxOuter.x - bbminOuter.x, bbmaxOuter.y - bbminOuter.y))) {
        ImGui::EndChild(); // Early exit if graph is not visible.
        return;
    }

    const float segmentSpacing = 0.0F;
    float segmentWidth = 1.0F;


    frameGraphInfo.maxFrameProfiles = INT_DIV_CEIL((size_t)(bbmaxInner.x - bbminInner.x), 100) * 100 + 200;
    float desiredSegmentWidth = (bbmaxInner.x - bbminInner.x) / (float)frameData.size();
    float minSegmentWidth = glm::max(1.0F, desiredSegmentWidth);

    float maxSegmentWidth = 10.0F;

    if (segmentWidth < minSegmentWidth) {
        segmentWidth = minSegmentWidth;
    }

    if (segmentWidth > maxSegmentWidth) {
        segmentWidth = maxSegmentWidth;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    const ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.7F, 0.7F, 0.7F, 1.0F));
    dl->AddRect(bbminOuter, bbmaxOuter, borderCol);

    ImGui::PushClipRect(bbminInner, bbmaxInner, true);

    float x0, y0, x1, y1;
    float topPadding = 8.0;

    float maxFrameTime = 1.0F;
//    if (frameGraphInfo.sortedFrameTimes.size() > 1) {
//        maxFrameTime = glm::max(maxFrameTime, (float) frameGraphInfo.sortedFrameTimes[1].first);
//    }

    std::vector<uint32_t> frameGraphRootPath;
    size_t rootProfileIndex = 0;
    uint32_t treeDepthLimit = 0;
    if (m_profilerDisplayMode == ProfilerDisplayMode_CallStack) {
        frameGraphRootPath = frameGraphInfo.frameGraphRootPath;
        treeDepthLimit = UINT32_MAX;
    }

    int32_t index = 0;
    for (auto it1 = frameData.rbegin(); it1 != frameData.rend(); ++it1, ++index) {
        if (index < 0)
            continue;

        x1 = bbmax.x - ((float)index * (segmentWidth + segmentSpacing) + padding);
        x0 = x1 - segmentWidth;

        if (x1 < bbmin.x)
            break;

        const FrameProfileData& frameProfileData = *it1;

        if (frameProfileData.profileData.empty())
            continue;

        if (frameProfileData.treeStructureChanged) {
            rootProfileIndex = getProfileIndexFromPath(frameProfileData.profileData, frameGraphRootPath);
            if (rootProfileIndex >= frameProfileData.profileData.size())
                rootProfileIndex = 0;
        }

//        maxFrameTime = glm::max(maxFrameTime, profileData[rootProfileIndex].elapsedMillis);

        y1 = bbmax.y;
        if (isGraphNormalized()) {
            y0 = bbmin.y;
        } else {
            y0 = y1 - (frameProfileData.profileData[rootProfileIndex].elapsedMillis / frameGraphInfo.heightScaleMsec) * (h - topPadding);
        }

        maxFrameTime = glm::max(maxFrameTime, (float)frameProfileData.profileData[rootProfileIndex].elapsedMillis);
        drawFrameSlice(frameProfileData.profileData, rootProfileIndex, x0, y0, x1, y1, treeDepthLimit);
    }

    drawFrameTimeOverlays(frameGraphInfo, bbminInner.x + padding, bbminInner.y, bbmaxInner.x, bbmaxInner.y);

    ImGui::PopClipRect();
    ImGui::EndChild();


    float newHeightScaleMsec = glm::ceil(maxFrameTime); // Round to the nearest millisecond
    float adjustmentFactor = glm::min((float)dt, 0.1F);

    frameGraphInfo.heightScaleMsec = glm::mix(frameGraphInfo.heightScaleMsec, newHeightScaleMsec, adjustmentFactor);
}

bool PerformanceGraphUI::drawFrameSlice(const std::vector<ProfileData>& profileData, const size_t& index, const float& x0, const float& y0, const float& x1, const float& y1, const uint32_t& treeDepthLimit) {
    const ProfileData& profile = profileData[index];
    const uint32_t& layerIndex = m_profilerDisplayMode == ProfilerDisplayMode_HotFunctionList ? profile.layerIndex : profile.pathIndex;
    const ProfileLayer& layer = m_layers[layerIndex];

    const float h = y1 - y0;
    float y = y1;

    if ((int)y == (int)y0) {
        return false;
    }

    if (treeDepthLimit > 0) {
        if (profile.hasChildren && layer.expanded) {
            size_t childIndex = index + 1; // First child is always the next element after its parent
            while (childIndex < profileData.size()) {
                const ProfileData& child = profileData[childIndex];
                double fraction = child.elapsedMillis / profile.elapsedMillis; // fraction of the parent height that this child takes up.

                float t1 = y;
                y -= h * (float)fraction;
                float t0 = y;

                drawFrameSlice(profileData, childIndex, x0, t0, x1, t1, treeDepthLimit - 1);

                childIndex = profileData[childIndex].nextSiblingIndex;
            }
        }
    }

    if ((int)y != (int)y0) {
        // At least one pixel of space was not filled by children, or this is a leaf that is larger than one pixel.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y), layer.colour);
    }

    return true;
}

void PerformanceGraphUI::drawFrameTimeOverlays(const FrameGraphInfo& frameGraphInfo, const float& xmin, const float& ymin, const float& xmax, const float& ymax) {

    if (isGraphNormalized()) {
        return; // Don't draw these absolute frame time values if the graph is normalized
    }

    const ImU32 frameTimeLineColour = ImGui::ColorConvertFloat4ToU32(ImVec4(0.5F, 0.5F, 0.5F, 0.5F));
    float x, y;
    char str[100] = {'\0'};

    const float h = ymax - ymin;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    x = xmin;
    y = ymin;
    sprintf_s(str, sizeof(str), "%02.1f ms (%.1f FPS)", frameGraphInfo.heightScaleMsec, 1000.0 / frameGraphInfo.heightScaleMsec);
    x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
    dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);

    x = xmin;
    y = ymax - h * 0.5F;
    sprintf_s(str, sizeof(str), "%02.1f ms (%.1f FPS)", frameGraphInfo.heightScaleMsec * 0.5, 1000.0 / (frameGraphInfo.heightScaleMsec * 0.5));
    x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
    dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);

    x = xmin;
    y = ymax - h * 0.25F;
    sprintf_s(str, sizeof(str), "%02.1f ms (%.1f FPS)", frameGraphInfo.heightScaleMsec * 0.25, 1000.0 / (frameGraphInfo.heightScaleMsec * 0.25));
    x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
    dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);

    if (m_showFrameTime90Percentile) {
        x = xmin;
        y = ymax - (float)(frameGraphInfo.frameTimePercentile90 / frameGraphInfo.heightScaleMsec) * h;
        sprintf_s(str, sizeof(str), "10%% %02.1f ms (%.1f FPS)", frameGraphInfo.frameTimePercentile90, 1000.0 / frameGraphInfo.frameTimePercentile90);
        x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
        dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);
    }

    if (m_showFrameTime99Percentile) {
        x = xmin;
        y = ymax - (float)(frameGraphInfo.frameTimePercentile99 / frameGraphInfo.heightScaleMsec) * h;
        sprintf_s(str, sizeof(str), "1%% %02.1f ms (%.1f FPS)", frameGraphInfo.frameTimePercentile99, 1000.0 / frameGraphInfo.frameTimePercentile99);
        x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
        dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);
    }

    if (m_showFrameTime999Percentile) {
        x = xmin;
        y = ymax - (float)(frameGraphInfo.frameTimePercentile999 / frameGraphInfo.heightScaleMsec) * h;
        sprintf_s(str, sizeof(str), "0.1%% %02.1f ms (%.1f FPS)", frameGraphInfo.frameTimePercentile999, 1000.0 / frameGraphInfo.frameTimePercentile999);
        x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
        dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);
    }

    constexpr bool useRollingAverage = true;
    double average = useRollingAverage ? frameGraphInfo.frameTimeAvg : frameGraphInfo.allFramesTimeAvg;

    x = xmin;
    y = ymax - (float)(average / frameGraphInfo.heightScaleMsec) * h;
    sprintf_s(str, sizeof(str), "AVG %02.1f ms (%.1f FPS)", average, 1000.0 / average);
    x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
    dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);

    if (ImGui::GetMousePos().x > xmin && ImGui::GetMousePos().x < xmax && ImGui::GetMousePos().y > ymin && ImGui::GetMousePos().y < ymax) {
        x = xmin;
        y = ImGui::GetMousePos().y;
        if (y > ymin && y < ymax) {
            float msec = (1.0F - ((y - ymin) / h)) * frameGraphInfo.heightScaleMsec;
            sprintf_s(str, sizeof(str), "POS %02.1f ms (%.1f FPS)", msec, 1000.0 / msec);
            x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
            dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);
        }
    }
}

float PerformanceGraphUI::drawFrameTimeOverlayText(const char* str, float x, float y, const float& xmin, const float& ymin, const float& xmax, const float& ymax) {
    ImVec2 size = ImGui::CalcTextSize(str);
    y -= (size.y / 2.0F);

    x = glm::min(glm::max(x, xmin), xmax - size.x);
    y = glm::min(glm::max(y, ymin), ymax - size.y);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + size.x, y + size.y), 0x88000000);
    ImGui::Text("%s", str);
    return size.x;
}


void PerformanceGraphUI::buildProfileTree(const FrameProfileData& frameData, FrameGraphInfo& frameGraphInfo, const size_t& index, const ProfileTreeSortOrder& sortOrder, std::vector<size_t>& tempReorderBuffer) {
    const ProfileData& rootProfile = frameData.profileData[0];
    const ProfileData& currProfile = frameData.profileData[index];

    assert(currProfile.layerIndex < m_layers.size());
    assert(currProfile.pathIndex < m_layers.size());

    ProfileLayer& uniqueLayer = m_layers[currProfile.layerIndex];
    ProfileLayer& pathLayer = m_layers[currProfile.pathIndex];
    const std::vector<uint32_t>& currentProfilePath = pathLayer.pathIndices;

    bool isCurrentRootPath = isProfilePathEqual(currentProfilePath, frameGraphInfo.frameGraphRootPath);

    ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!currProfile.hasChildren)
        treeNodeFlags |= ImGuiTreeNodeFlags_Leaf;
    if (isCurrentRootPath)
        treeNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Selected;
    if (!m_profileNameFilterSearchTerms.empty()) {
        if (uniqueLayer.passedTreeFilter)
            treeNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
        else {
            return; // This branch of the tree is filtered out.
        }
    }

    std::string id = std::string("##") + std::to_string(currProfile.pathIndex);
    pathLayer.expanded = ImGui::TreeNodeEx((uniqueLayer.layerName + id).c_str(), treeNodeFlags);

    ImVec4 tempColour = ImGui::ColorConvertU32ToFloat4(pathLayer.colour);

    ImGui::SameLine();
    ImGui::NextColumn(); // CPU time
    ImGui::Text("%.2f msec", currProfile.elapsedMillis);
    ImGui::NextColumn(); // CPU %
    ImGui::Text("%.2f %%", currProfile.elapsedMillis / rootProfile.elapsedMillis * 100.0);
    ImGui::NextColumn(); // Colour
    if (ImGui::ColorEdit3(id.c_str(), &tempColour.x, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs))
        pathLayer.colour = ImGui::ColorConvertFloat4ToU32(tempColour);
    ImGui::NextColumn(); // Visible
    if (currProfile.hasChildren) {
        if (ImGui::RadioButton(id.c_str(), isCurrentRootPath))
            frameGraphInfo.frameGraphRootPath = currentProfilePath;
    }
    ImGui::NextColumn();


    if (pathLayer.expanded) {
        if (currProfile.hasChildren) {
            tempReorderBuffer.clear();
            size_t childIndex = index + 1; // First child is always the next element after its parent
            while (childIndex < frameData.profileData.size()) {
                if (sortOrder == ProfileTreeSortOrder_Default) {
                    // Traverse to children right now
                    buildProfileTree(frameData, frameGraphInfo, childIndex, sortOrder, tempReorderBuffer);
                } else {
                    // Store child indices to be traversed after sorting
                    tempReorderBuffer.emplace_back(childIndex);
                }
                childIndex = frameData.profileData[childIndex].nextSiblingIndex;
            }

            if (sortOrder != ProfileTreeSortOrder_Default) {
                if (sortOrder == ProfileTreeSortOrder_CpuTimeDescending) {
                    std::sort(tempReorderBuffer.begin(), tempReorderBuffer.end(), [&frameData](const size_t& lhs, const size_t& rhs) {
                        return frameData.profileData[lhs].elapsedMillis > frameData.profileData[rhs].elapsedMillis;
                    });
                }
                if (sortOrder == ProfileTreeSortOrder_CpuTimeAscending) {
                    std::sort(tempReorderBuffer.begin(), tempReorderBuffer.end(), [&frameData](const size_t& lhs, const size_t& rhs) {
                        return frameData.profileData[lhs].elapsedMillis < frameData.profileData[rhs].elapsedMillis;
                    });
                }

                for (const size_t& sequenceIndex : tempReorderBuffer) {
                    buildProfileTree(frameData, frameGraphInfo, sequenceIndex, sortOrder, tempReorderBuffer);
                }
            }
        }
        ImGui::TreePop();
    }
}

void PerformanceGraphUI::initializeProfileDataTree(FrameProfileData* currentFrame, const Profiler::Profile* profiles, const size_t& count, const size_t& stride) {
    PROFILE_SCOPE("PerformanceGraphUI::initializeProfileDataTree")
    currentFrame->profileData.clear();
    currentFrame->profileData.reserve(count);

    uint16_t parentOffset;

    const uint8_t* rawPtr = reinterpret_cast<const uint8_t*>(profiles);
    size_t offset = 0;

    for (size_t i = 0; i < count; ++i, offset += stride) {
        const Profiler::Profile& profile = *reinterpret_cast<const Profiler::Profile*>(rawPtr + offset);
//        const Profiler::Profile& profile = profiles[i];

        bool hasChildren = profile.lastChildIndex != SIZE_MAX;

        parentOffset = 0;

        if (profile.parentIndex != SIZE_MAX) {
            assert(i > profile.parentIndex);
            assert((i - profile.parentIndex) <= UINT16_MAX); // Is this a reasonable limit, or is it possible that a valid use case of recursion could break this?
            parentOffset = (uint16_t)(i - profile.parentIndex);
        }

        PerformanceGraphUI::ProfileData& profileData = currentFrame->profileData.emplace_back();
        profileData.nextSiblingIndex = profile.nextSiblingIndex; // TODO: store 16-bit nextSiblingOffset instead
        profileData.parentOffset = parentOffset;
        profileData.hasChildren = hasChildren;
    }
}

void PerformanceGraphUI::initializeProfileTreeLayers(FrameProfileData* currentFrame, const Profiler::Profile* profiles, const size_t& count, const size_t& stride, std::vector<uint32_t>& layerPath) {
    PROFILE_SCOPE("PerformanceGraphUI::initializeProfileTreeLayers")

    layerPath.clear();
    size_t parentIndex = SIZE_MAX;
    uint32_t popCount;

    const uint8_t* rawPtr = reinterpret_cast<const uint8_t*>(profiles);

    for (size_t i = 0; i < count; ++i) {
        const Profiler::Profile& profile = *reinterpret_cast<const Profiler::Profile*>(rawPtr + i * stride);
        ProfileData& profileData = currentFrame->profileData[i];

        const uint32_t& layerIndex = getUniqueLayerIndex(profile.id->name);
        popCount = 0;
        // Profile tree is sorted depth-first, we pop until we reach the root or the common parent wit the previous node
        while (parentIndex != SIZE_MAX && profile.parentIndex <= parentIndex) {
            const Profiler::Profile& parentProfile = *reinterpret_cast<const Profiler::Profile*>(rawPtr + parentIndex * stride);
            parentIndex = parentProfile.parentIndex;
            ++popCount;
        }
        layerPath.resize(layerPath.size() - popCount);
        layerPath.emplace_back(layerIndex);
        profileData.layerIndex = layerIndex;

        profileData.pathIndex = getPathLayerIndex(layerPath);

        parentIndex = profile.parentIndex;
    }
}

void PerformanceGraphUI::checkFrameProfileTreeStructureChanged(FrameProfileData* currentFrame, FrameProfileData* previousFrame) {
    PROFILE_SCOPE("PerformanceGraphUI::checkFrameProfileTreeStructureChanged")
    if (previousFrame == nullptr) {
        currentFrame->treeStructureChanged = true;
    } else if (currentFrame->profileData.size() != previousFrame->profileData.size()) {
        // Ideally in most circumstances, this will be the fast path taken. Presumably most successive frames will have the same function call structure
        currentFrame->treeStructureChanged = true;
    } else {
        currentFrame->treeStructureChanged = false;
        for (size_t i = 0; i < currentFrame->profileData.size(); ++i) {
            PerformanceGraphUI::ProfileData& currProfileData = currentFrame->profileData[i];
            PerformanceGraphUI::ProfileData& prevProfileData = previousFrame->profileData[i];
            if (currProfileData.layerIndex != prevProfileData.layerIndex) {
                currentFrame->treeStructureChanged = true;
                break;
            }
        }
    }
}

void PerformanceGraphUI::updateFrameGraphInfo(FrameGraphInfo& frameGraphInfo, const float& rootElapsed) {
    PROFILE_SCOPE("PerformanceGraphUI::updateFrameGraphInfo")
    frameGraphInfo.frameTimes.emplace_back(rootElapsed);

    frameGraphInfo.frameTimeSum += rootElapsed;
//    frameGraphInfo.frameTimeRollingAvg = glm::lerp(frameGraphInfo.frameTimeRollingAvg, (double)rootElapsed, frameGraphInfo.frameTimeRollingAvgDecayRate);

    auto insertPos = std::upper_bound(frameGraphInfo.sortedFrameTimes.begin(), frameGraphInfo.sortedFrameTimes.end(), rootElapsed, [](const float& lhs, const std::pair<float, uint32_t>& rhs) {
        return lhs > rhs.first;
    });
    frameGraphInfo.sortedFrameTimes.insert(insertPos, std::make_pair(rootElapsed, frameGraphInfo.frameSequence++));

    size_t count999 = frameGraphInfo.frameTimes.size() / 1000;
    size_t count99 = frameGraphInfo.frameTimes.size() / 100;
    size_t count90 = frameGraphInfo.frameTimes.size() / 10;

    size_t maxSortedFrameTimes = glm::max(count90 * 2, (size_t)(500));
    while (frameGraphInfo.sortedFrameTimes.size() > maxSortedFrameTimes) {
        auto it1 = frameGraphInfo.sortedFrameTimes.begin();
        for (auto it2 = it1 + 1; it2 != frameGraphInfo.sortedFrameTimes.end(); ++it2) {
            if (it2->second < it1->second)
                it1 = it2; // Find the location of the oldest frame (the smallest sequence) and remove it.
        }
        assert(it1 != frameGraphInfo.sortedFrameTimes.end()); // sortedFrameTimes is too big, we must remove one frame.
        frameGraphInfo.sortedFrameTimes.erase(it1);
    }

    frameGraphInfo.frameTimePercentile999 = frameGraphInfo.frameTimePercentile99 = frameGraphInfo.frameTimePercentile90 = 0.0;
    if (count999 > 0) {
        for (size_t i = 0; i < count999; ++i) frameGraphInfo.frameTimePercentile999 += frameGraphInfo.sortedFrameTimes[i].first;
        frameGraphInfo.frameTimePercentile999 /= (float)count999;
    }
    if (count99 > 0) {
        for (size_t i = 0; i < count99; ++i) frameGraphInfo.frameTimePercentile99 += frameGraphInfo.sortedFrameTimes[i].first;
        frameGraphInfo.frameTimePercentile99 /= (float)count99;
    }
    if (count90 > 0) {
        for (size_t i = 0; i < count90; ++i) frameGraphInfo.frameTimePercentile90 += frameGraphInfo.sortedFrameTimes[i].first;
        frameGraphInfo.frameTimePercentile90 /= (float)count90;
    }

    frameGraphInfo.allFramesTimeAvg = 0.0;
    for (size_t i = 0; i < frameGraphInfo.frameTimes.size(); ++i)
        frameGraphInfo.allFramesTimeAvg += frameGraphInfo.frameTimes[i];
    frameGraphInfo.allFramesTimeAvg /= (float)frameGraphInfo.frameTimes.size();
}

void PerformanceGraphUI::updateAccumulatedAverages() {
    PROFILE_SCOPE("PerformanceGraphUI::updateAccumulatedAverages")
    ++m_averageAccumulationFrameCount;

    float invAverageDivisor = 1.0F / (float)m_averageAccumulationFrameCount;

    auto currentTime = Performance::now();
    auto elapsedTime = currentTime - m_lastAverageAccumulationTime;
    if (elapsedTime >= m_averageAccumulationDuration || m_lastAverageAccumulationTime == Performance::zero_moment) {
        m_lastAverageAccumulationTime = currentTime;
        m_averageAccumulationFrameCount = 0;

        float newAverage;

        for (ProfileLayer& layer : m_layers) {
            newAverage = layer.accumulatedSelfTimeSum * invAverageDivisor;
            layer.accumulatedSelfTimeAvg = glm::lerp(newAverage, layer.accumulatedSelfTimeAvg, m_rollingAverageUpdateFactor);
            layer.accumulatedSelfTimeSum = 0.0F;

            newAverage = layer.accumulatedTotalTimeSum * invAverageDivisor;
            layer.accumulatedTotalTimeAvg = glm::lerp(newAverage, layer.accumulatedTotalTimeAvg, m_rollingAverageUpdateFactor);
            layer.accumulatedTotalTimeSum = 0.0F;
        }

        for (const auto& [threadId, threadProfiles] : m_currentThreadProfiles) {
            FrameGraphInfo& threadInfo = m_threadFrameGraphInfo[threadId];
            newAverage = threadInfo.frameTimeSum * invAverageDivisor;
            threadInfo.frameTimeAvg = glm::lerp(newAverage, threadInfo.frameTimeAvg, m_rollingAverageUpdateFactor);
            threadInfo.frameTimeSum = 0.0F;
        }
        newAverage = m_gpuFrameGraphInfo.frameTimeSum * invAverageDivisor;
        m_gpuFrameGraphInfo.frameTimeAvg = glm::lerp(newAverage, m_gpuFrameGraphInfo.frameTimeAvg, m_rollingAverageUpdateFactor);
        m_gpuFrameGraphInfo.frameTimeSum = 0.0F;
    }

}

void PerformanceGraphUI::flushOldFrames() {
    PROFILE_SCOPE("PerformanceGraphUI::flushOldFrames")
    size_t totalFlushed = 0;

    for (auto it = m_threadFrameProfileData.begin(); it != m_threadFrameProfileData.end(); ++it)
        totalFlushed += Util::removeVectorOverflowStart(it->second, m_maxFrameProfiles);
    totalFlushed += Util::removeVectorOverflowStart(m_gpuFrameProfileData, m_maxFrameProfiles);

    size_t maxThreadInfoFrameTimes = 10000;
    for (auto& it : m_threadFrameGraphInfo)
        totalFlushed += Util::removeVectorOverflowStart(it.second.frameTimes, maxThreadInfoFrameTimes);
    totalFlushed += Util::removeVectorOverflowStart(m_gpuFrameGraphInfo.frameTimes, m_maxFrameProfiles);
}

double PerformanceGraphUI::calculateFrameTimePercentile(const std::vector<double>& frameTimes, std::deque<double>& tempContainer, double percentile) {
    PROFILE_SCOPE("PerformanceGraphUI::calculateFrameTimePercentile")

    tempContainer.clear();

    const size_t frameCount = glm::max((size_t)1, (size_t)((double)frameTimes.size() * percentile));

    for (const double& frameTime : frameTimes) {
        auto insertPos = std::upper_bound(tempContainer.begin(), tempContainer.end(), frameTime, std::greater<>());

        if (insertPos != tempContainer.begin() || tempContainer.size() < frameCount)
            tempContainer.insert(insertPos, frameTime);

        if (tempContainer.size() >= frameCount)
            tempContainer.pop_front();
    }

    double sum = 0.0;

    for (const double& frameTime : tempContainer) {
        sum += frameTime;
    }

    return sum / (double)tempContainer.size();
}

const uint32_t& PerformanceGraphUI::getUniqueLayerIndex(const std::string& layerName) {
//    PROFILE_SCOPE("PerformanceGraphUI::getUniqueLayerIndex")
    auto it = m_uniqueLayerIndexMap.find(layerName);
    if (it == m_uniqueLayerIndexMap.end()) {
        uint32_t index = (uint32_t)m_layers.size();
        ProfileLayer& layer = m_layers.emplace_back();
        size_t nameHash = std::hash<std::string>()(layerName); // Same colour every time for same input. Hash is random enough for the colour.
        layer.colour = ((uint32_t)nameHash) | IM_COL32_A_MASK;
//        layer.colour = Util::random<uint32_t>(0, UINT32_MAX) | IM_COL32_A_MASK;
        layer.expanded = false;
        layer.visible = true;
        layer.layerName = layerName;
        layer.accumulatedSelfTime = 0.0F;
        layer.accumulatedSelfTimeAvg = 0.0F;
        layer.accumulatedSelfTimeSum = 0.0F;
        layer.accumulatedTotalTime = 0.0F;
        layer.accumulatedTotalTimeAvg = 0.0F;
        layer.accumulatedTotalTimeSum = 0.0F;
        it = m_uniqueLayerIndexMap.insert(std::make_pair(layerName, index)).first;
    }
    return it->second;
}

const uint32_t& PerformanceGraphUI::getPathLayerIndex(const std::vector<uint32_t>& layerPath) {
//    PROFILE_SCOPE("PerformanceGraphUI::getPathLayerIndex")
    auto it = m_pathLayerIndexMap.find(layerPath);
    if (it == m_pathLayerIndexMap.end()) {
        uint32_t index = (uint32_t)m_layers.size();
        ProfileLayer& layer = m_layers.emplace_back();
        size_t pathHash = std::hash<std::vector<uint32_t>>()(layerPath); // Same colour every time for same input. Hash is random enough for the colour.
        layer.colour = ((uint32_t)pathHash) | IM_COL32_A_MASK;
//        layer.colour = Util::random<uint32_t>(0, UINT32_MAX) | IM_COL32_A_MASK;
        layer.expanded = false;
        layer.visible = true;
//        layer.layerName = std::to_string(index);
        layer.pathIndices = layerPath;
        layer.accumulatedSelfTime = 0.0F;
        layer.accumulatedSelfTimeAvg = 0.0F;
        layer.accumulatedSelfTimeSum = 0.0F;
        layer.accumulatedTotalTime = 0.0F;
        layer.accumulatedTotalTimeAvg = 0.0F;
        layer.accumulatedTotalTimeSum = 0.0F;
        layer.layerName = "";
        for (size_t i = 0; i < layerPath.size(); ++i) {
            layer.layerName += m_layers[layerPath[i]].layerName;
            if (i < layerPath.size() - 1)
                layer.layerName += "\\\\";
        }
        it = m_pathLayerIndexMap.insert(std::make_pair(layerPath, index)).first;
    }
    return it->second;
}

bool PerformanceGraphUI::isGraphNormalized() const {
    return m_graphsNormalized && m_profilerDisplayMode == ProfilerDisplayMode_CallStack;
}

bool PerformanceGraphUI::isProfilePathEqual(const std::vector<uint32_t>& path1, const std::vector<uint32_t>& path2) {
    if (path1.empty() || path2.empty())
        return false;
    if (path1.size() != path2.size())
        return false;
    for (size_t i = 0; i < path1.size(); ++i) {
        if (path1[i] != path2[i])
            return false;
    }
    return true;
}

size_t PerformanceGraphUI::getProfileIndexFromPath(const std::vector<ProfileData>& profileData, const std::vector<uint32_t>& path) {
    if (path.empty())
        return SIZE_MAX;

    size_t currentIndex = 0;
    for (size_t i = 0; i < path.size() && currentIndex < profileData.size(); ++i) {
        currentIndex = findChildIndexForLayer(profileData, currentIndex, path[i]);
    }

    return currentIndex;
}

size_t PerformanceGraphUI::findChildIndexForLayer(const std::vector<ProfileData>& profileData, const size_t& parentProfileIndex, const uint32_t& layerIndex) {
    if (profileData[parentProfileIndex].hasChildren) {
        size_t childIndex = parentProfileIndex + 1;
        while (childIndex < profileData.size()) {
            if (profileData[childIndex].layerIndex == layerIndex)
                return childIndex;
            childIndex = profileData[childIndex].nextSiblingIndex;
        }
    }
    return SIZE_MAX;
}

void PerformanceGraphUI::updateFilteredLayerPaths(const std::vector<ProfileData>& profileData) {
    PROFILE_SCOPE("PerformanceGraphUI::updateFilteredLayerPaths")

    for (const ProfileData& profile : profileData) {
        ProfileLayer& uniqueLayer = m_layers[profile.layerIndex];
        uniqueLayer.passedTreeFilter = matchSearchTerms(uniqueLayer.layerName, m_profileNameFilterSearchTerms);

        if (uniqueLayer.passedTreeFilter) {
            ProfileLayer& pathLayer = m_layers[profile.pathIndex];
            for (const uint32_t& index :  pathLayer.pathIndices) {
                m_layers[index].passedTreeFilter = true;
            }
        }
    }
}

bool PerformanceGraphUI::matchSearchTerms(const std::string& str, const std::vector<std::string>& searchTerms) {
//    PROFILE_SCOPE("PerformanceGraphUI::matchSearchTerms")
    if (searchTerms.empty())
        return true;

    for (const std::string& searchTerm : searchTerms) {
        auto it = std::search(str.begin(), str.end(), searchTerm.begin(), searchTerm.end(), [](const char& lhs, const char& rhs) {
            return tolower(lhs) == tolower(rhs);
        });
        if (it != str.end())
            return true;
    }

    return false;
}