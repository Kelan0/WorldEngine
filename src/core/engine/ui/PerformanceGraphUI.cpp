#include "core/engine/ui/PerformanceGraphUI.h"
#include "core/application/Application.h"
#include "core/util/Profiler.h"
#include "core/util/Util.h"
#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_internal.h"

PerformanceGraphUI::PerformanceGraphUI():
    m_firstFrame(true),
    m_profilingPaused(false),
    m_graphsNormalized(false),
    m_clearFrames(false),
    m_showFrameTime90Percentile(false),
    m_showFrameTime99Percentile(false),
    m_showFrameTime999Percentile(false),
    m_graphVisible(0),
    m_heightScaleMsec(5.0),
    m_maxFrameProfiles(500),
    m_frameGraphRootIndex(0) {
}

PerformanceGraphUI::~PerformanceGraphUI() = default;

void PerformanceGraphUI::update(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::update");

    if (m_clearFrames) {
        m_clearFrames = false;
        for (auto& [threadId, allProfiles] : m_frameProfileData) {
            allProfiles.clear();
        }

        for (auto& [threadId, frameTimeInfo] : m_threadFrameTimeInfo) {
            frameTimeInfo.frameTimes.clear();
            frameTimeInfo.sortedFrameTimes.clear();
        }
    }

    if (m_profilingPaused) {
        return;
    }

    for (auto it = m_currentFrameProfile.begin(); it != m_currentFrameProfile.end(); ++it) {
        it->second.clear(); // Keeps the array allocated for the next frame
    }

    Profiler::getFrameProfile(m_currentFrameProfile);


    for (auto it = m_currentFrameProfile.begin(); it != m_currentFrameProfile.end(); ++it) {
        const uint64_t& threadId = it->first;
        const ThreadProfile& threadProfile = it->second;
        FrameTimeInfo& threadInfo = m_threadFrameTimeInfo[threadId];
        std::vector<FrameProfileData>& allProfiles = m_frameProfileData[threadId];


        FrameProfileData& currentFrame = allProfiles.emplace_back();
        currentFrame.profileData.clear();

        currentFrame.profileData.reserve(threadProfile.size());

        std::vector<uint32_t> layerPath;
        size_t parentIndex = SIZE_MAX;
        size_t popCount = 0;

        for (size_t i = 0; i < threadProfile.size(); ++i) {
            const Profiler::CPUProfile& profile = threadProfile[i];
            bool hasChildren = profile.lastChildIndex != SIZE_MAX;

            const uint32_t& layerIndex = getUniqueLayerIndex(profile.id->name);

            popCount = 0;
            while (parentIndex != SIZE_MAX && profile.parentIndex <= parentIndex) {
                parentIndex = threadProfile[parentIndex].parentIndex;
                ++popCount;
            }

            layerPath.resize(layerPath.size() - popCount);
            layerPath.emplace_back(layerIndex);

            const uint32_t& pathIndex = getPathLayerIndex(layerPath);

            ProfileData& profileData = currentFrame.profileData.emplace_back();
            profileData.layerIndex = layerIndex;
            profileData.pathIndex = pathIndex;
            profileData.elapsedCPU = (float)Performance::milliseconds(profile.startTime, profile.endTime);
            profileData.nextSiblingIndex = profile.nextSiblingIndex;
            profileData.hasChildren = hasChildren;

            parentIndex = profile.parentIndex;
        }

        float rootElapsed = (float)Performance::milliseconds(threadProfile[0].startTime, threadProfile[0].endTime);
        threadInfo.frameTimes.emplace_back(rootElapsed);

        auto insertPos = std::upper_bound(threadInfo.sortedFrameTimes.begin(), threadInfo.sortedFrameTimes.end(), rootElapsed, [](const float& lhs, const std::pair<float, uint32_t>& rhs) {
            return lhs > rhs.first;
        });
        threadInfo.sortedFrameTimes.insert(insertPos, std::make_pair(rootElapsed, threadInfo.frameSequence++));

        size_t count999 = glm::max((size_t)1, threadInfo.frameTimes.size() / 1000);
        size_t count99 = glm::max((size_t)1, threadInfo.frameTimes.size() / 100);
        size_t count90 = glm::max((size_t)1, threadInfo.frameTimes.size() / 10);

        size_t maxSortedFrameTimes = glm::max(count90 * 2, (size_t)(500));
        while (threadInfo.sortedFrameTimes.size() > maxSortedFrameTimes) {
            auto it1 = threadInfo.sortedFrameTimes.begin();
            for (auto it2 = it1 + 1; it2 != threadInfo.sortedFrameTimes.end(); ++it2) {
                if (it2->second < it1->second)
                    it1 = it2; // Find the location of the oldest frame (the smallest sequence) and remove it.
            }
            assert(it1 != threadInfo.sortedFrameTimes.end()); // sortedFrameTimes is too big, we must remove one frame.
            threadInfo.sortedFrameTimes.erase(it1);
        }

        threadInfo.frameTimePercentile999 = threadInfo.frameTimePercentile99 = threadInfo.frameTimePercentile90 = 0.0;
        for (size_t i = 0; i < count90; ++i) {
            if (i < count999) threadInfo.frameTimePercentile999 += threadInfo.sortedFrameTimes[i].first;
            if (i < count99) threadInfo.frameTimePercentile99 += threadInfo.sortedFrameTimes[i].first;
            if (i < count90) threadInfo.frameTimePercentile90 += threadInfo.sortedFrameTimes[i].first;
        }

        threadInfo.frameTimePercentile999 /= (double)count999;
        threadInfo.frameTimePercentile99 /= (double)count99;
        threadInfo.frameTimePercentile90 /= (double)count90;

        threadInfo.frameTimeAvg = 0.0;
        for (size_t i = 0; i < threadInfo.frameTimes.size(); ++i)
            threadInfo.frameTimeAvg += threadInfo.frameTimes[i];
        threadInfo.frameTimeAvg /= (double)threadInfo.frameTimes.size();
    }

    flushOldFrames();
}

void PerformanceGraphUI::draw(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::draw");

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;

    ImGui::Begin("Profiler");
    drawHeaderBar();
    ImGui::Separator();
    drawProfileContent(dt);
    ImGui::End();

    m_firstFrame = false;
}

void PerformanceGraphUI::drawHeaderBar() {
    PROFILE_SCOPE("PerformanceGraphUI::drawHeaderBar");

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
    PROFILE_SCOPE("PerformanceGraphUI::drawProfileContent");

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;

    ImGui::BeginGroup();
    {
        ImGui::BeginColumns("frameGraph-profileTree", 2);
        {
            drawProfileTree(dt);

            ImGui::NextColumn();

            drawFrameGraphs(dt);
        }
        ImGui::EndColumns();
    }
    ImGui::EndGroup();
}

void PerformanceGraphUI::drawProfileTree(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::drawProfileTree");

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;


    ImGui::BeginChild("profileTree");
    {
        ImGui::BeginColumns("profileTree", 7, ImGuiColumnsFlags_NoPreserveWidths);
        {
            // TODO: this should only be initialized once
            float x = ImGui::GetWindowContentRegionMax().x;
            ImGui::SetColumnOffset(7, x);
            x -= 60;
            ImGui::SetColumnOffset(6, x);
            x -= 60;
            ImGui::SetColumnOffset(5, x);
            x -= 80;
            ImGui::SetColumnOffset(4, x);
            x -= 100;
            ImGui::SetColumnOffset(3, x);
            x -= 80;
            ImGui::SetColumnOffset(2, x);
            x -= 100;
            ImGui::SetColumnOffset(1, x);

            if (x >= 0) {
                ImGui::Text("Profile Name");
                ImGui::NextColumn();
                ImGui::Text("CPU Time");
                ImGui::NextColumn();
                ImGui::Text("CPU %%");
                ImGui::NextColumn();
                ImGui::Text("GPU Time");
                ImGui::NextColumn();
                ImGui::Text("GPU %%");
                ImGui::NextColumn();
                ImGui::Text("Colour");
                ImGui::NextColumn();
                ImGui::Text("Focus");
                ImGui::NextColumn();

                auto it = m_frameProfileData.find(Application::instance()->getHashedMainThreadId());
                if (it != m_frameProfileData.end() && !it->second.empty()) {
                    const FrameProfileData& currentFrame = it->second.back();
                    std::vector<size_t> temp;
                    buildProfileTree(currentFrame.profileData, 0, ProfileTreeSortOrder::Default, temp);
                }
            }
        }
        ImGui::EndColumns();
    }
    ImGui::EndChild();
}

void PerformanceGraphUI::drawFrameGraphs(const double& dt) {
    PROFILE_SCOPE("PerformanceGraphUI::drawFrameGraphs");

    ImGui::BeginChild("FrameGraphContainer");
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    uint64_t threadId = Application::instance()->getHashedMainThreadId();

    auto it = m_frameProfileData.find(threadId);
    if (it == m_frameProfileData.end() || it->second.empty())
        return; // Nothing to show

    const std::vector<FrameProfileData>& mainThreadFrames = it->second;

    auto it1 = m_threadFrameTimeInfo.find(threadId);
    if (it1 == m_threadFrameTimeInfo.end())
        return;
    const FrameTimeInfo& threadInfo = it1->second;

    const float padding = 2.0F;

    float x = window->Pos.x;
    float y = window->Pos.y;
    float w = window->Size.x;// / 2;
    float h = window->Size.y;

    drawFrameGraph(dt, "CPU_FrameGraph", mainThreadFrames, threadInfo, x, y, w, h, padding);
    ImGui::EndChild();
}

void PerformanceGraphUI::drawFrameGraph(const double& dt, const char* strId, const std::vector<FrameProfileData>& frameData, const FrameTimeInfo& frameTimeInfo, const float& x, const float& y, const float& w, const float& h, const float& padding) {
    PROFILE_SCOPE("PerformanceGraphUI::drawFrameGraph");

    ImGui::BeginChild(strId);
    const float margin = 0.0F;
    const float segmentSpacing = 0.0F;
    float segmentWidth = 1.0F;


    if (w < (padding + margin) * 2 || h < (padding + margin) * 2)
        return;

    ImVec2 bbmin = ImVec2(x, y);
    ImVec2 bbmax = ImVec2(x + w, y + h);
    ImVec2 bbminInner = ImVec2(bbmin.x + padding, bbmin.y + padding);
    ImVec2 bbmaxInner = ImVec2(bbmax.x - padding, bbmax.y - padding);
    ImVec2 bbminOuter = ImVec2(bbmin.x - margin, bbmin.y - margin);
    ImVec2 bbmaxOuter = ImVec2(bbmax.x + margin, bbmax.y + margin);

    m_maxFrameProfiles = INT_DIV_CEIL((size_t)(bbmaxInner.x - bbminInner.x), 100) * 100 + 200;
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
    if (frameTimeInfo.sortedFrameTimes.size() > 1) {
        maxFrameTime = glm::max(maxFrameTime, (float) frameTimeInfo.sortedFrameTimes[1].first);
    }

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

//        maxFrameTime = glm::max(maxFrameTime, profileData[m_frameGraphRootIndex].elapsedCPU);
    
        y1 = bbmax.y;
        if (m_graphsNormalized) {
            y0 = bbmin.y;
        } else {
            y0 = y1 - (profileData[m_frameGraphRootIndex].elapsedCPU / m_heightScaleMsec) * (h - topPadding);
        }
    
        drawFrameSlice(profileData, m_frameGraphRootIndex, x0, y0, x1, y1);
    }

    drawFrameTimeOverlays(frameTimeInfo, bbminInner.x + padding, bbminInner.y, bbmaxInner.x, bbmaxInner.y);

    ImGui::PopClipRect();
    ImGui::EndChild();

    float newHeightScaleMsec = glm::ceil(maxFrameTime / 2.0F) * 2.0F;
    float adjustmentFactor = glm::min((float)dt, 0.1F);
    m_heightScaleMsec = glm::mix(m_heightScaleMsec, newHeightScaleMsec, adjustmentFactor);
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
            double fraction = child.elapsedCPU / profile.elapsedCPU; // fraction of the parent height that this child takes up.

            float t1 = y;
            y -= h * (float)fraction;
            float t0 = y;

            drawFrameSlice(profileData, childIndex, x0, t0, x1, t1);

            childIndex = profileData[childIndex].nextSiblingIndex;
        }
    }

    if ((int)y != (int)y0) {
        // At least one pixel of space was not filled by children, or this is a leaf that is larger than one pixel.
        ImU32 colour = ImGui::ColorConvertFloat4ToU32(ImVec4(layer.colour[0], layer.colour[1], layer.colour[2], 1.0F));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y), colour);
    }

    return true;
}

void PerformanceGraphUI::drawFrameTimeOverlays(const FrameTimeInfo& frameTimeInfo, const float& xmin, const float& ymin, const float& xmax, const float& ymax) {

    if (m_graphsNormalized) {
        return; // Don't draw these absolite frame time values if the graph is notmalized
    }

    const ImU32 frameTimeLineColour = ImGui::ColorConvertFloat4ToU32(ImVec4(0.5F, 0.5F, 0.5F, 0.5F));
    float x, y;
    char str[100] = { '\0' };

    const float h = ymax - ymin;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    x = xmin;
    y = ymin;
    sprintf_s(str, sizeof(str), "%02.1f ms (%.1f FPS)", m_heightScaleMsec, 1000.0 / m_heightScaleMsec);
    x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
    dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);

    x = xmin;
    y = ymax - h * 0.5F;
    sprintf_s(str, sizeof(str), "%02.1f ms (%.1f FPS)", m_heightScaleMsec * 0.5, 1000.0 / (m_heightScaleMsec * 0.5));
    x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
    dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);

    x = xmin;
    y = ymax - h * 0.25F;
    sprintf_s(str, sizeof(str), "%02.1f ms (%.1f FPS)", m_heightScaleMsec * 0.25, 1000.0 / (m_heightScaleMsec * 0.25));
    x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
    dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);

    if (m_showFrameTime90Percentile) {
        x = xmin;
        y = ymax - (float)(frameTimeInfo.frameTimePercentile90 / m_heightScaleMsec) * h;
        sprintf_s(str, sizeof(str), "10%% %02.1f ms (%.1f FPS)", frameTimeInfo.frameTimePercentile90, 1000.0 / frameTimeInfo.frameTimePercentile90);
        x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
        dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);
    }

    if (m_showFrameTime99Percentile) {
        x = xmin;
        y = ymax - (float)(frameTimeInfo.frameTimePercentile99 / m_heightScaleMsec) * h;
        sprintf_s(str, sizeof(str), "1%% %02.1f ms (%.1f FPS)", frameTimeInfo.frameTimePercentile99, 1000.0 / frameTimeInfo.frameTimePercentile99);
        x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
        dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);
    }

    if (m_showFrameTime999Percentile) {
        x = xmin;
        y = ymax - (float)(frameTimeInfo.frameTimePercentile999 / m_heightScaleMsec) * h;
        sprintf_s(str, sizeof(str), "0.1%% %02.1f ms (%.1f FPS)", frameTimeInfo.frameTimePercentile999, 1000.0 / frameTimeInfo.frameTimePercentile999);
        x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
        dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);
    }

    x = xmin;
    y = ymax - (float)(frameTimeInfo.frameTimeAvg / m_heightScaleMsec) * h;
    sprintf_s(str, sizeof(str), "AVG %02.1f ms (%.1f FPS)", frameTimeInfo.frameTimeAvg, 1000.0 / frameTimeInfo.frameTimeAvg);
    x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
    dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);

    if (ImGui::GetMousePos().x > xmin && ImGui::GetMousePos().x < xmax && ImGui::GetMousePos().y > ymin && ImGui::GetMousePos().y < ymax) {
        x = xmin;
        y = ImGui::GetMousePos().y;
        if (y > ymin && y < ymax) {
            float msec = (1.0F - ((y - ymin) / h)) * m_heightScaleMsec;
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


void PerformanceGraphUI::buildProfileTree(const std::vector<ProfileData>& profiles, const size_t& index, const ProfileTreeSortOrder& sortOrder, std::vector<size_t>& tempReorderBuffer) {
    const ProfileData& rootProfile = profiles[0];
    const ProfileData& currProfile = profiles[index];

    assert(currProfile.layerIndex < m_layers.size());
    assert(currProfile.pathIndex < m_layers.size());

    ProfileLayer& layer = m_layers[currProfile.layerIndex];
    ProfileLayer& pathLayer = m_layers[currProfile.pathIndex];

    ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!currProfile.hasChildren)
        treeNodeFlags |= ImGuiTreeNodeFlags_Leaf;
    if (index == m_frameGraphRootIndex)
        treeNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Selected;

    pathLayer.expanded = ImGui::TreeNodeEx((layer.layerName + pathLayer.layerName).c_str(), treeNodeFlags);

    ImGui::SameLine();
    ImGui::NextColumn(); // CPU time
    ImGui::Text("%.2f msec", currProfile.elapsedCPU);
    ImGui::NextColumn(); // CPU %
    ImGui::Text("%.2f %%", currProfile.elapsedCPU / rootProfile.elapsedCPU * 100.0);
    ImGui::NextColumn(); // GPU time
    ImGui::Text("%.2f msec", 0.0F);
    ImGui::NextColumn(); // GPU %
    ImGui::Text("%.2f %%", 0.0F);
    ImGui::NextColumn(); // Colour
    ImGui::ColorEdit3(pathLayer.layerName.c_str(), &pathLayer.colour[0], ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs);
    ImGui::NextColumn(); // Visible
    if (currProfile.hasChildren) {
        if (ImGui::RadioButton(pathLayer.layerName.c_str(), m_frameGraphRootIndex == index))
            m_frameGraphRootIndex = index;
    }
    ImGui::NextColumn();


    if (pathLayer.expanded) {
        if (currProfile.hasChildren) {
            tempReorderBuffer.clear();
            size_t childIndex = index + 1; // First child is always the next element after its parent
            while (childIndex < profiles.size()) {
                if (sortOrder == ProfileTreeSortOrder::Default) {
                    // Traverse to children right now
                    buildProfileTree(profiles, childIndex, sortOrder, tempReorderBuffer);
                } else {
                    // Store child indices to be traversed after sorting
                    tempReorderBuffer.emplace_back(childIndex);
                }
                childIndex = profiles[childIndex].nextSiblingIndex;
            }

            if (sortOrder != ProfileTreeSortOrder::Default) {
                if (sortOrder == ProfileTreeSortOrder::CpuTimeDescending) {
                    std::sort(tempReorderBuffer.begin(), tempReorderBuffer.end(), [&profiles](const size_t& lhs, const size_t& rhs) {
                        return profiles[lhs].elapsedCPU > profiles[rhs].elapsedCPU;
                    });
                }
                if (sortOrder == ProfileTreeSortOrder::CpuTimeAscending) {
                    std::sort(tempReorderBuffer.begin(), tempReorderBuffer.end(), [&profiles](const size_t& lhs, const size_t& rhs) {
                        return profiles[lhs].elapsedCPU < profiles[rhs].elapsedCPU;
                    });
                }

                for (const size_t& sequenceIndex : tempReorderBuffer) {
                    buildProfileTree(profiles, sequenceIndex, sortOrder, tempReorderBuffer);
                }
            }
        }
        ImGui::TreePop();
    }
}

void PerformanceGraphUI::flushOldFrames() {
    PROFILE_SCOPE("PerformanceGraphUI::flushOldFrames");
    size_t totalFlushed = 0;

    for (auto it = m_frameProfileData.begin(); it != m_frameProfileData.end(); ++it) {
        totalFlushed += Util::removeVectorOverflowStart(it->second, m_maxFrameProfiles);
    }

    size_t maxThreadInfoFrameTimes = 10000;
    for (auto& it : m_threadFrameTimeInfo) {
        totalFlushed += Util::removeVectorOverflowStart(it.second.frameTimes, maxThreadInfoFrameTimes);
    }
}

double PerformanceGraphUI::calculateFrameTimePercentile(const std::vector<double>& frameTimes, std::deque<double>& tempContainer, double percentile) {
    PROFILE_SCOPE("PerformanceGraphUI::calculateFrameTimePercentile");

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
    auto it = m_uniqueLayerIndexMap.find(layerName);
    if (it == m_uniqueLayerIndexMap.end()) {
        uint32_t index = (uint32_t)m_layers.size();
        ProfileLayer& layer = m_layers.emplace_back();
        layer.colour[0] = (float)rand() / RAND_MAX;
        layer.colour[1] = (float)rand() / RAND_MAX;
        layer.colour[2] = (float)rand() / RAND_MAX;
        layer.expanded = false;
        layer.visible = true;
        layer.layerName = layerName;
        it = m_uniqueLayerIndexMap.insert(std::make_pair(layerName, index)).first;
    }
    return it->second;
}

const uint32_t& PerformanceGraphUI::getPathLayerIndex(const std::vector<uint32_t>& layerPath) {
    auto it = m_pathLayerIndexMap.find(layerPath);
    if (it == m_pathLayerIndexMap.end()) {
        uint32_t index = (uint32_t)m_layers.size();
        ProfileLayer& layer = m_layers.emplace_back();
        layer.colour[0] = (float)rand() / RAND_MAX;
        layer.colour[1] = (float)rand() / RAND_MAX;
        layer.colour[2] = (float)rand() / RAND_MAX;
        layer.expanded = false;
        layer.visible = true;
        layer.layerName = std::string("##") + std::to_string(index);
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

