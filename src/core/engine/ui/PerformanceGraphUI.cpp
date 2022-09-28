#include "core/engine/ui/PerformanceGraphUI.h"
#include "core/application/Application.h"
#include "core/util/Profiler.h"
#include "core/util/Util.h"
#include "core/imgui/imgui.h"
#include "core/imgui/imgui_internal.h"

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
    m_maxFrameProfiles(500) {
}

PerformanceGraphUI::~PerformanceGraphUI() {
}

void PerformanceGraphUI::update(double dt) {
    PROFILE_SCOPE("PerformanceGraphUI::update");

    if (m_clearFrames) {
        m_clearFrames = false;
        for (auto it = m_frameProfileData.begin(); it != m_frameProfileData.end(); ++it) {
            std::vector<FrameProfileData>& allProfiles = it->second;
            allProfiles.clear();
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
        ThreadInfo& threadInfo = m_threadInfo[threadId];
        std::vector<FrameProfileData>& allProfiles = m_frameProfileData[threadId];

        FrameProfileData& currentFrame = allProfiles.emplace_back();
        currentFrame.profileData.clear();

        currentFrame.profileData.reserve(threadProfile.size());

        for (size_t i = 0; i < threadProfile.size(); ++i) {
            const Profiler::Profile& profile = threadProfile[i];
            bool hasChildren = profile.lastChildIndex != SIZE_MAX;

            ProfileData& profileData = currentFrame.profileData.emplace_back();
            profileData.layerIndex = getUniqueLayerIndex(profile.id->strA);
            profileData.elapsedCPU = Performance::milliseconds(profile.startCPU, profile.endCPU);
            profileData.parentIndex = profile.parentIndex;
            profileData.nextSiblingIndex = profile.nextSiblingIndex;
            profileData.hasChildren = hasChildren;
        }

        float rootElapsed = (float)Performance::milliseconds(threadProfile[0].startCPU, threadProfile[0].endCPU);
        threadInfo.frameTimes.emplace_back(rootElapsed);

        auto insertPos = std::upper_bound(threadInfo.sortedFrameTimes.begin(), threadInfo.sortedFrameTimes.end(), rootElapsed, std::greater<double>());
        threadInfo.sortedFrameTimes.insert(insertPos, rootElapsed);

        size_t count999 = glm::max((size_t)1, threadInfo.frameTimes.size() / 1000);
        size_t count99 = glm::max((size_t)1, threadInfo.frameTimes.size() / 100);
        size_t count90 = glm::max((size_t)1, threadInfo.frameTimes.size() / 10);

        size_t maxSortedFrameTimes = count90 * 2;
        if (threadInfo.sortedFrameTimes.size() > maxSortedFrameTimes) {
            int64_t eraseCount = threadInfo.sortedFrameTimes.size() - maxSortedFrameTimes;
            threadInfo.sortedFrameTimes.erase(threadInfo.sortedFrameTimes.begin(), threadInfo.sortedFrameTimes.begin() + eraseCount);
        }

        threadInfo.frameTimePercentile999 = threadInfo.frameTimePercentile99 = threadInfo.frameTimePercentile90 = 0.0;
        for (size_t i = 0; i < count90; ++i) {
            if (i < count999) threadInfo.frameTimePercentile999 += threadInfo.sortedFrameTimes[i];
            if (i < count99) threadInfo.frameTimePercentile99 += threadInfo.sortedFrameTimes[i];
            if (i < count90) threadInfo.frameTimePercentile90 += threadInfo.sortedFrameTimes[i];
        }

        threadInfo.frameTimePercentile999 /= count999;
        threadInfo.frameTimePercentile99 /= count99;
        threadInfo.frameTimePercentile90 /= count90;

        threadInfo.frameTimeAvg = 0.0;
        for (size_t i = 0; i < threadInfo.frameTimes.size(); ++i)
            threadInfo.frameTimeAvg += threadInfo.frameTimes[i];
        threadInfo.frameTimeAvg /= threadInfo.frameTimes.size();
    }

    flushOldFrames();
}

void PerformanceGraphUI::draw(double dt) {
    PROFILE_SCOPE("PerformanceGraphUI::draw");

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;

    ImGui::Begin("Profiler");
    drawHeaderBar();
    ImGui::Separator();
    drawProfileContent();
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

void PerformanceGraphUI::drawProfileContent() {
    PROFILE_SCOPE("PerformanceGraphUI::drawProfileContent");

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;

    ImGui::BeginGroup();
    {
        ImGui::BeginColumns("frameGraph-profileTree", 2);
        {
            drawProfileTree();

            ImGui::NextColumn();

            drawFrameGraph();
        }
        ImGui::EndColumns();
    }
    ImGui::EndGroup();
}

void PerformanceGraphUI::drawProfileTree() {
    PROFILE_SCOPE("PerformanceGraphUI::drawProfileTree");

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Size.x < 10 || window->Size.y < 10)
        return;


    ImGui::BeginChild("profileTree");
    {
        ImGui::BeginColumns("profileTree", 6, ImGuiColumnsFlags_NoPreserveWidths);
        {
            // TODO: this should only be initialized once
            float x = ImGui::GetWindowContentRegionMax().x;
            //ImGui::SetColumnOffset(7, x);
            //x -= 60;
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
                //ImGui::Text("Visible");
                //ImGui::NextColumn();

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

void PerformanceGraphUI::drawFrameGraph() {
    PROFILE_SCOPE("PerformanceGraphUI::drawFrameGraph");

    uint64_t threadId = Application::instance()->getHashedMainThreadId();

    auto it = m_frameProfileData.find(threadId);
    if (it == m_frameProfileData.end() || it->second.empty())
        return; // Nothing to show

    const std::vector<FrameProfileData>& allFrames = it->second;

    ImGui::BeginChild("frameGraph");
    const float padding = 1.0F;
    const float margin = 1.0F;
    const float segmentSpacing = 0.0F;
    float segmentWidth = 1.0F;

    ImGuiWindow* window = ImGui::GetCurrentWindow();

    ImVec2 bbsize = ImVec2(window->Size.x, window->Size.y);
    if (bbsize.x < (padding + margin) * 2 || bbsize.y < (padding + margin) * 2)
        return;

    ImGui::PushItemWidth(-1);

    ImVec2 bbmin = ImVec2(window->Pos.x, window->Pos.y);
    ImVec2 bbmax = ImVec2(bbmin.x + bbsize.x, bbmin.y + bbsize.y);
    ImVec2 bbminInner = ImVec2(bbmin.x + padding, bbmin.y + padding);
    ImVec2 bbmaxInner = ImVec2(bbmax.x - padding, bbmax.y - padding);
    ImVec2 bbminOuter = ImVec2(bbmin.x - margin, bbmin.y - margin);
    ImVec2 bbmaxOuter = ImVec2(bbmax.x + margin, bbmax.y + margin);

    m_maxFrameProfiles = INT_DIV_CEIL(bbmaxInner.x - bbminInner.x, 100) * 100 + 200;
    float desiredSegmentWidth = (bbmaxInner.x - bbminInner.x) / allFrames.size();
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

    float maxFrameTime = 2.0;

    int32_t index = 0;
    for (auto it1 = allFrames.rbegin(); it1 != allFrames.rend(); ++it1, ++index) {
        if (index < 0)
            continue;
    
        x1 = bbmax.x - (index * (segmentWidth + segmentSpacing) + padding);
        x0 = x1 - segmentWidth;
    
        if (x1 < bbmin.x)
            break;
    
        const std::vector<ProfileData>& frameData = it1->profileData;

        if (frameData.empty())
            continue;

        maxFrameTime = glm::max(maxFrameTime, frameData[0].elapsedCPU);
    
        y1 = bbmax.y;
        if (m_graphsNormalized) {
            y0 = bbmin.y;
        } else {
            y0 = y1 - (frameData[0].elapsedCPU / m_heightScaleMsec) * (bbsize.y - topPadding);
        }
    
        drawFrameSlice(frameData, 0, x0, y0, x1, y1);
    }

    drawFrameTimeOverlays(threadId, bbminInner.x + padding, bbminInner.y, bbmaxInner.x, bbmaxInner.y);

    ImGui::PopClipRect();

    ImGui::EndChild();

    float newHeightScaleMsec = glm::ceil(maxFrameTime / 2.0) * 2.0;
    float adjustmentFactor = 0.05;
    m_heightScaleMsec = m_heightScaleMsec * (1.0F - adjustmentFactor) + newHeightScaleMsec * adjustmentFactor;
}

void PerformanceGraphUI::buildFrameSlice(const std::vector<ProfileData>& profileData, const size_t& index, const float& y0, const float& y1, std::vector<FrameGraphSlice>& outSlice) {

    //const ProfileData& profile = profileData[index];
    //const ProfileLayer& layer = m_layers[profile.layerIndex];
    //
    //const float h = y1 - y0;
    //float y = y1;
    //
    //if (profile.hasChildren && layer.expanded) {
    //    size_t childIndex = index + 1; // First child is always the next element after its parent
    //    while (childIndex < profileData.size()) {
    //        const ProfileData& child = profileData[childIndex];
    //        double fraction = child.elapsedCPU / profile.elapsedCPU; // fraction of the parent height that this child takes up.
    //
    //        float t1 = y;
    //        y -= h * (float)fraction;
    //        float t0 = y;
    //
    //        buildFrameSlice(profileData, childIndex, t0, t1, outSlice);
    //
    //        childIndex = profileData[childIndex].nextSiblingIndex;
    //    }
    //}
    //
    //if ((int)y != (int)y0) {
    //    // At least one pixel of space was not filled by children, or this is a leaf that is larger than one pixel.
    //    FrameSegment& segment = outSegment.emplace_back();
    //    segment.y0 = y0;
    //    segment.y1 = y;
    //    segment.colour = ImGui::ColorConvertFloat4ToU32(ImVec4(layer.colour[0], layer.colour[1], layer.colour[2], 1.0F));
    //}
}


void PerformanceGraphUI::drawFrameSlice(const std::vector<ProfileData>& profileData, const size_t& index, const float& x0, const float& y0, const float& x1, const float& y1) {

    const ProfileData& profile = profileData[index];
    const ProfileLayer& layer = m_layers[profile.layerIndex];
    
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
}

void PerformanceGraphUI::drawFrameTimeOverlays(const uint64_t& threadId, const float& xmin, const float& ymin, const float& xmax, const float& ymax) {

    if (m_graphsNormalized) {
        return; // Don't draw these absolite frame time values if the graph is notmalized
    }

    auto it = m_threadInfo.find(threadId);
    if (it == m_threadInfo.end())
        return;
    const ThreadInfo& threadInfo = it->second;

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
    y = ymax - h * 0.5;
    sprintf_s(str, sizeof(str), "%02.1f ms (%.1f FPS)", m_heightScaleMsec * 0.5, 1000.0 / (m_heightScaleMsec * 0.5));
    x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
    dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);

    x = xmin;
    y = ymax - h * 0.25;
    sprintf_s(str, sizeof(str), "%02.1f ms (%.1f FPS)", m_heightScaleMsec * 0.25, 1000.0 / (m_heightScaleMsec * 0.25));
    x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
    dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);

    if (m_showFrameTime90Percentile) {
        x = xmin;
        y = ymax - (threadInfo.frameTimePercentile90 / m_heightScaleMsec) * h;
        sprintf_s(str, sizeof(str), "10%% %02.1f ms (%.1f FPS)", threadInfo.frameTimePercentile90, 1000.0 / threadInfo.frameTimePercentile90);
        x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
        dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);
    }

    if (m_showFrameTime99Percentile) {
        x = xmin;
        y = ymax - (threadInfo.frameTimePercentile99 / m_heightScaleMsec) * h;
        sprintf_s(str, sizeof(str), "1%% %02.1f ms (%.1f FPS)", threadInfo.frameTimePercentile99, 1000.0 / threadInfo.frameTimePercentile99);
        x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
        dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);
    }

    if (m_showFrameTime999Percentile) {
        x = xmin;
        y = ymax - (threadInfo.frameTimePercentile999 / m_heightScaleMsec) * h;
        sprintf_s(str, sizeof(str), "0.1%% %02.1f ms (%.1f FPS)", threadInfo.frameTimePercentile999, 1000.0 / threadInfo.frameTimePercentile999);
        x += drawFrameTimeOverlayText(str, x, y, xmin, ymin, xmax, ymax);
        dl->AddLine(ImVec2(x, y), ImVec2(xmax, y), frameTimeLineColour);
    }

    x = xmin;
    y = ymax - (threadInfo.frameTimeAvg / m_heightScaleMsec) * h;
    sprintf_s(str, sizeof(str), "AVG %02.1f ms (%.1f FPS)", threadInfo.frameTimeAvg, 1000.0 / threadInfo.frameTimeAvg);
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

    ProfileLayer& layer = m_layers[currProfile.layerIndex];

    ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!currProfile.hasChildren)
        treeNodeFlags |= ImGuiTreeNodeFlags_Leaf;

    layer.expanded = ImGui::TreeNodeEx(layer.layerName.c_str(), treeNodeFlags);

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
    ImGui::ColorEdit3(layer.layerName.c_str(), &layer.colour[0], ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs);
    ImGui::NextColumn(); // Visible
    //ImGui::Checkbox("Show", &layer.visible);
    //ImGui::NextColumn();

    if (layer.expanded) {
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

                for (const size_t& index : tempReorderBuffer) {
                    buildProfileTree(profiles, index, sortOrder, tempReorderBuffer);
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
    for (auto& it : m_threadInfo) {
        totalFlushed += Util::removeVectorOverflowStart(it.second.frameTimes, maxThreadInfoFrameTimes);
    }
}

double PerformanceGraphUI::calculateFrameTimePercentile(const std::vector<double>& frameTimes, std::deque<double>& tempContainer, double percentile) {
    PROFILE_SCOPE("PerformanceGraphUI::calculateFrameTimePercentile");

    tempContainer.clear();

    const size_t frameCount = glm::max((size_t)1, (size_t)((double)frameTimes.size() * percentile));

    for (const double& frameTime : frameTimes) {
        auto insertPos = std::upper_bound(tempContainer.begin(), tempContainer.end(), frameTime, std::greater<double>());

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
        uint32_t index = m_layers.size();
        ProfileLayer& layer = m_layers.emplace_back();
        layer.colour[0] = (float)rand() / RAND_MAX;
        layer.colour[1] = (float)rand() / RAND_MAX;
        layer.colour[2] = (float)rand() / RAND_MAX;
        layer.expanded = false;
        layer.layerName = layerName;
        it = m_uniqueLayerIndexMap.insert(std::make_pair(layerName, index)).first;
    }
    return it->second;
}

//const uint32_t& PerformanceGraphUI::getPathLayerIndex(const std::vector<uint32_t>& layerPath) {
//    auto it = m_pathLayerIndexMap.find(layerPath);
//    if (it == m_pathLayerIndexMap.end()) {
//        uint32_t index = m_layers.size();
//        ProfileLayer& layer = m_layers.emplace_back();
//        layer.colour[0] = (float)rand() / RAND_MAX;
//        layer.colour[1] = (float)rand() / RAND_MAX;
//        layer.colour[2] = (float)rand() / RAND_MAX;
//        layer.expanded = false;
//        layer.layerName = "";
//        it = m_pathLayerIndexMap.insert(std::make_pair(layerPath, index)).first;
//    }
//    return it->second;
//}

