#include "core/engine/ui/PerformanceGraphUI.h"
#include "core/application/Application.h"
#include "core/util/Profiler.h"
#include "core/util/Util.h"
#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_internal.h"

PerformanceGraphUI::PerformanceGraphUI():
        m_profilingPaused(false),
        m_graphsNormalized(false),
        m_clearFrames(true),
        m_showFrameTime90Percentile(false),
        m_showFrameTime99Percentile(false),
        m_showFrameTime999Percentile(false),
        m_graphVisible(0),
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
            frameGraphInfo.frameTimeAvg = 0.0;
            frameGraphInfo.frameTimeRollingAvg = 0.0;
            frameGraphInfo.frameTimePercentile90 = 0.0;
            frameGraphInfo.frameTimePercentile99 = 0.0;
            frameGraphInfo.frameTimePercentile999 = 0.0;
            frameGraphInfo.frameSequence = 0;
        }

        m_gpuFrameGraphInfo.frameTimes.clear();
        m_gpuFrameGraphInfo.sortedFrameTimes.clear();
        m_gpuFrameGraphInfo.frameTimeAvg = 0.0;
        m_gpuFrameGraphInfo.frameTimeRollingAvg = 0.0;
        m_gpuFrameGraphInfo.frameTimePercentile90 = 0.0;
        m_gpuFrameGraphInfo.frameTimePercentile99 = 0.0;
        m_gpuFrameGraphInfo.frameTimePercentile999 = 0.0;
        m_gpuFrameGraphInfo.frameSequence = 0;
    }

    if (m_profilingPaused) {
        return;
    }

    PROFILE_REGION("Get thread profile data")
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

    PROFILE_REGION("Process thread profile data")
    for (const auto& [threadId, threadProfiles] : m_currentThreadProfiles) {
        FrameGraphInfo& threadInfo = m_threadFrameGraphInfo[threadId];
        std::vector<FrameProfileData>& allProfiles = m_threadFrameProfileData[threadId];


        FrameProfileData& currentFrame = allProfiles.emplace_back();

        initializeProfileDataTree(currentFrame, threadProfiles.data(), threadProfiles.size(), sizeof(ThreadProfile));

        initializeProfileTreeLayers(currentFrame, threadProfiles.data(), threadProfiles.size(), sizeof(ThreadProfile), layerPath);

        for (size_t i = 0; i < threadProfiles.size(); ++i) {
            const ThreadProfile& profile = threadProfiles[i];
            currentFrame.profileData[i].elapsedMillis = (float)Performance::milliseconds(profile.startTime, profile.endTime);
        }

        if (threadId == mainThreadId) {
            for (size_t i = 0; i < currentFrame.profileData.size(); ++i) {
                const ProfileData& profile = currentFrame.profileData[i];
                ProfileLayer& uniqueLayer = m_layers[profile.layerIndex];
                uniqueLayer.accumulatedTotalTime += profile.elapsedMillis;
                uniqueLayer.accumulatedSelfTime += profile.elapsedMillis;

//                ProfileLayer& pathLayer = m_layers[profile.pathIndex];
//                pathLayer.accumulatedTotalTime += profile.elapsedMillis;
//                pathLayer.accumulatedSelfTime += profile.elapsedMillis;

                if (profile.parentOffset != 0) {
                    ProfileData& parentProfile = currentFrame.profileData[i - profile.parentOffset];
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

    PROFILE_REGION("Get GPU profile data")
    m_currentGpuProfiles.clear();

    if (Profiler::getLatestGpuFrameProfile(m_currentGpuProfiles)) {
        PROFILE_REGION("Process GPU profile data")
        FrameProfileData& currentFrame = m_gpuFrameProfileData.emplace_back();

        initializeProfileDataTree(currentFrame, m_currentGpuProfiles.data(), m_currentGpuProfiles.size(), sizeof(GPUProfile));

        initializeProfileTreeLayers(currentFrame, m_currentGpuProfiles.data(), m_currentGpuProfiles.size(), sizeof(GPUProfile), layerPath);

        for (size_t i = 0; i < m_currentGpuProfiles.size(); ++i) {
            const GPUProfile& profile = m_currentGpuProfiles[i];
            currentFrame.profileData[i].elapsedMillis = (float)(profile.endQuery.time - profile.startQuery.time);
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

    ImGui::BeginGroup();
    {
        ImGui::Checkbox("Pause", &m_profilingPaused);
        ImGui::SameLine(0.0F, 10.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine(0.0F, 10.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine(0.0F, 10.0F);
        ImGui::Checkbox("Normalize", &m_graphsNormalized);

        ImGui::SameLine(0.0F, 10.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);


        static const char* GRAPH_VISIBILITY_OPTIONS[] = {"Both Graphs", "CPU Graph", "GPU Graph"};

        ImGui::SameLine(0.0F, 10.0F);
        ImGui::PushItemWidth(120);
        if (ImGui::BeginCombo("Visible", GRAPH_VISIBILITY_OPTIONS[m_graphVisible], ImGuiComboFlags_PopupAlignLeft)) {
            if (ImGui::Selectable(GRAPH_VISIBILITY_OPTIONS[0], m_graphVisible == 0)) m_graphVisible = 0;
            if (ImGui::Selectable(GRAPH_VISIBILITY_OPTIONS[1], m_graphVisible == 1)) m_graphVisible = 1;
            if (ImGui::Selectable(GRAPH_VISIBILITY_OPTIONS[2], m_graphVisible == 2)) m_graphVisible = 2;
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::SameLine(0.0F, 10.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine(0.0F, 10.0F);
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
//            drawProfileTree(dt);
            drawUniqueLayerList(dt);

            ImGui::NextColumn();

            drawFrameGraphs(dt);
        }
        ImGui::EndColumns();
    }
    ImGui::EndGroup();
}

void PerformanceGraphUI::drawProfileTree(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::drawProfileTree")

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
                    buildProfileTree(currentFrame, frameGraphInfo, 0, ProfileTreeSortOrder::Default, temp);
                }
            }
        }
        ImGui::EndColumns();
    }
    ImGui::EndChild();
}

void PerformanceGraphUI::drawUniqueLayerList(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::drawUniqueLayerList")

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

                PROFILE_REGION("Allocate layer indices")
                std::vector<size_t> uniqueLayerIndices;
                uniqueLayerIndices.emplace_back(rootProfile.layerIndex);

                uniqueLayerIndices.reserve(m_uniqueLayerIndexMap.size() + 1);
                for (const auto& [layerName, layerIndex] : m_uniqueLayerIndexMap)
                    uniqueLayerIndices.emplace_back(layerIndex);


                PROFILE_REGION("Sort layers")
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
                PROFILE_REGION("Display layers")

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
    float h = window->Size.y / 2 - padding * 2;


    auto it0 = m_threadFrameProfileData.find(threadId);
    auto it1 = m_threadFrameGraphInfo.find(threadId);
    if (it0 != m_threadFrameProfileData.end() && it1 != m_threadFrameGraphInfo.end()) {
        const std::vector<FrameProfileData>& mainThreadFrameData = it0->second;
        FrameGraphInfo& mainThreadFrameGraphInfo = it1->second;
        drawFrameGraph(dt, "CPU_FrameGraph", mainThreadFrameData, mainThreadFrameGraphInfo, x, y, w, h, padding);

        m_gpuFrameGraphInfo.heightScaleMsec = mainThreadFrameGraphInfo.heightScaleMsec;
    }

    y += h + padding;

    drawFrameGraph(dt, "GPU_FrameGraph", m_gpuFrameProfileData, m_gpuFrameGraphInfo, x, y, w, h, padding);

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

    int32_t index = 0;
    for (auto it1 = frameData.rbegin(); it1 != frameData.rend(); ++it1, ++index) {
        if (index < 0)
            continue;

        x1 = bbmax.x - ((float)index * (segmentWidth + segmentSpacing) + padding);
        x0 = x1 - segmentWidth;

        if (x1 < bbmin.x)
            break;

        const std::vector<ProfileData>& profileData = it1->profileData;

        if (profileData.empty())
            continue;

//        maxFrameTime = glm::max(maxFrameTime, profileData[m_frameGraphRootIndex].elapsedMillis);

        y1 = bbmax.y;
        if (m_graphsNormalized) {
            y0 = bbmin.y;
        } else {
            y0 = y1 - (profileData[frameGraphInfo.frameGraphRootIndex].elapsedMillis / frameGraphInfo.heightScaleMsec) * (h - topPadding);
        }

        maxFrameTime = glm::max(maxFrameTime, (float)profileData[frameGraphInfo.frameGraphRootIndex].elapsedMillis);
        drawFrameSlice(profileData, frameGraphInfo.frameGraphRootIndex, x0, y0, x1, y1);
    }

    drawFrameTimeOverlays(frameGraphInfo, bbminInner.x + padding, bbminInner.y, bbmaxInner.x, bbmaxInner.y);

    ImGui::PopClipRect();
    ImGui::EndChild();

    float newHeightScaleMsec = glm::ceil(maxFrameTime); // Round to the nearest millisecond
//    float adjustmentFactor = glm::min((float)dt, 0.1F);
    frameGraphInfo.heightScaleMsec = glm::mix(frameGraphInfo.heightScaleMsec, newHeightScaleMsec, frameGraphInfo.frameTimeRollingAvgDecayRate);
}

bool PerformanceGraphUI::drawFrameSlice(const std::vector<ProfileData>& profileData, const size_t& index, const float& x0, const float& y0, const float& x1, const float& y1) {
    const ProfileData& profile = profileData[index];
    const ProfileLayer& layer = m_layers[profile.pathIndex];

    const float h = y1 - y0;
    float y = y1;

    if (profile.hasChildren && layer.expanded) {
        size_t childIndex = index + 1; // First child is always the next element after its parent
        while (childIndex < profileData.size()) {
            const ProfileData& child = profileData[childIndex];
            double fraction = child.elapsedMillis / profile.elapsedMillis; // fraction of the parent height that this child takes up.

            float t1 = y;
            y -= h * (float)fraction;
            float t0 = y;

            drawFrameSlice(profileData, childIndex, x0, t0, x1, t1);

            childIndex = profileData[childIndex].nextSiblingIndex;
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

    if (m_graphsNormalized) {
        return; // Don't draw these absolite frame time values if the graph is notmalized
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
    double average = useRollingAverage ? frameGraphInfo.frameTimeRollingAvg : frameGraphInfo.frameTimeAvg;

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

    ProfileLayer& layer = m_layers[currProfile.layerIndex];
    ProfileLayer& pathLayer = m_layers[currProfile.pathIndex];

    ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!currProfile.hasChildren)
        treeNodeFlags |= ImGuiTreeNodeFlags_Leaf;
    if (index == frameGraphInfo.frameGraphRootIndex)
        treeNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Selected;

    std::string id = std::string("##") + layer.layerName + pathLayer.layerName;
    pathLayer.expanded = ImGui::TreeNodeEx(id.c_str(), treeNodeFlags);

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
        if (ImGui::RadioButton(id.c_str(), frameGraphInfo.frameGraphRootIndex == index))
            frameGraphInfo.frameGraphRootIndex = index;
    }
    ImGui::NextColumn();


    if (pathLayer.expanded) {
        if (currProfile.hasChildren) {
            tempReorderBuffer.clear();
            size_t childIndex = index + 1; // First child is always the next element after its parent
            while (childIndex < frameData.profileData.size()) {
                if (sortOrder == ProfileTreeSortOrder::Default) {
                    // Traverse to children right now
                    buildProfileTree(frameData, frameGraphInfo, childIndex, sortOrder, tempReorderBuffer);
                } else {
                    // Store child indices to be traversed after sorting
                    tempReorderBuffer.emplace_back(childIndex);
                }
                childIndex = frameData.profileData[childIndex].nextSiblingIndex;
            }

            if (sortOrder != ProfileTreeSortOrder::Default) {
                if (sortOrder == ProfileTreeSortOrder::CpuTimeDescending) {
                    std::sort(tempReorderBuffer.begin(), tempReorderBuffer.end(), [&frameData](const size_t& lhs, const size_t& rhs) {
                        return frameData.profileData[lhs].elapsedMillis > frameData.profileData[rhs].elapsedMillis;
                    });
                }
                if (sortOrder == ProfileTreeSortOrder::CpuTimeAscending) {
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

void PerformanceGraphUI::initializeProfileDataTree(FrameProfileData& currentFrame, const Profiler::Profile* profiles, const size_t& count, const size_t& stride) {
    PROFILE_SCOPE("PerformanceGraphUI::initializeProfileDataTree")
    currentFrame.profileData.clear();
    currentFrame.profileData.reserve(count);

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

        PerformanceGraphUI::ProfileData& profileData = currentFrame.profileData.emplace_back();
        profileData.nextSiblingIndex = profile.nextSiblingIndex; // TODO: store 16-bit nextSiblingOffset instead
        profileData.parentOffset = parentOffset;
        profileData.hasChildren = hasChildren;
    }
}

void PerformanceGraphUI::initializeProfileTreeLayers(FrameProfileData& currentFrame, const Profiler::Profile* profiles, const size_t& count, const size_t& stride, std::vector<uint32_t>& layerPath) {
    PROFILE_SCOPE("PerformanceGraphUI::initializeProfileTreeLayers")

    layerPath.clear();
    size_t parentIndex = SIZE_MAX;
    uint32_t popCount;

    const uint8_t* rawPtr = reinterpret_cast<const uint8_t*>(profiles);

    for (size_t i = 0; i < count; ++i) {
        const Profiler::Profile& profile = *reinterpret_cast<const Profiler::Profile*>(rawPtr + i * stride);
        ProfileData& profileData = currentFrame.profileData[i];

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

void PerformanceGraphUI::updateFrameGraphInfo(FrameGraphInfo& frameGraphInfo, const float& rootElapsed) {
    PROFILE_SCOPE("PerformanceGraphUI::updateFrameGraphInfo")
    frameGraphInfo.frameTimes.emplace_back(rootElapsed);

    frameGraphInfo.frameTimeRollingAvg = glm::lerp(frameGraphInfo.frameTimeRollingAvg, (double)rootElapsed, frameGraphInfo.frameTimeRollingAvgDecayRate);

    auto insertPos = std::upper_bound(frameGraphInfo.sortedFrameTimes.begin(), frameGraphInfo.sortedFrameTimes.end(), rootElapsed, [](const float& lhs, const std::pair<float, uint32_t>& rhs) {
        return lhs > rhs.first;
    });
    frameGraphInfo.sortedFrameTimes.insert(insertPos, std::make_pair(rootElapsed, frameGraphInfo.frameSequence++));

    size_t count999 = glm::max((size_t)1, frameGraphInfo.frameTimes.size() / 1000);
    size_t count99 = glm::max((size_t)1, frameGraphInfo.frameTimes.size() / 100);
    size_t count90 = glm::max((size_t)1, frameGraphInfo.frameTimes.size() / 10);

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
    for (size_t i = 0; i < count90; ++i) {
        if (i < count999) frameGraphInfo.frameTimePercentile999 += frameGraphInfo.sortedFrameTimes[i].first;
        if (i < count99) frameGraphInfo.frameTimePercentile99 += frameGraphInfo.sortedFrameTimes[i].first;
        if (i < count90) frameGraphInfo.frameTimePercentile90 += frameGraphInfo.sortedFrameTimes[i].first;
    }

    frameGraphInfo.frameTimePercentile999 /= (double)count999;
    frameGraphInfo.frameTimePercentile99 /= (double)count99;
    frameGraphInfo.frameTimePercentile90 /= (double)count90;

    frameGraphInfo.frameTimeAvg = 0.0;
    for (size_t i = 0; i < frameGraphInfo.frameTimes.size(); ++i)
        frameGraphInfo.frameTimeAvg += frameGraphInfo.frameTimes[i];
    frameGraphInfo.frameTimeAvg /= (double)frameGraphInfo.frameTimes.size();
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

            newAverage = layer.accumulatedTotalTimeSum * invAverageDivisor;
            layer.accumulatedTotalTimeAvg = glm::lerp(newAverage, layer.accumulatedTotalTimeAvg, m_rollingAverageUpdateFactor);

            layer.accumulatedSelfTimeSum = 0.0F;
            layer.accumulatedTotalTimeSum = 0.0F;
        }
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
        layer.colour = Util::random<uint32_t>(0, UINT32_MAX) | IM_COL32_A_MASK;
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
        layer.colour = Util::random<uint32_t>(0, UINT32_MAX) | IM_COL32_A_MASK;
        layer.expanded = false;
        layer.visible = true;
        layer.layerName = std::to_string(index);
        layer.pathIndices = layerPath;
        layer.accumulatedSelfTime = 0.0F;
        layer.accumulatedSelfTimeAvg = 0.0F;
        layer.accumulatedSelfTimeSum = 0.0F;
        layer.accumulatedTotalTime = 0.0F;
        layer.accumulatedTotalTimeAvg = 0.0F;
        layer.accumulatedTotalTimeSum = 0.0F;
        //layer.layerName = "";
        //for (size_t i = 0; i < layerPath.size(); ++i) {
        //    if (i > 0)
        //        layer.layerName += "..";
        //    layer.layerName += m_layers[layerPath[i]].layerName;
        //}
        it = m_pathLayerIndexMap.insert(std::make_pair(layerPath, index)).first;
    }
    return it->second;
}

