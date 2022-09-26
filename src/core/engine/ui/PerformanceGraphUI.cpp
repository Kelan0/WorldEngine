#include "core/engine/ui/PerformanceGraphUI.h"
#include "core/application/Application.h"
#include "core/util/Profiler.h"
#include "core/imgui/imgui.h"
#include "core/imgui/imgui_internal.h"

PerformanceGraphUI::PerformanceGraphUI():
    m_firstFrame(true),
    m_profilingPaused(false),
    m_showIdleFrameTime(false),
    m_graphsNormalized(false),
    m_graphPacked(false),
    m_clearFrames(false),
    m_graphVisible(0),
    m_heightScaleMsec(5.0),
    m_maxFrameProfiles(500) {
}

PerformanceGraphUI::~PerformanceGraphUI() {
}

void PerformanceGraphUI::draw(double dt) {
    PROFILE_SCOPE("PerformanceGraphUI::draw");
    updateProfileData();


    FrameProfile& frameProfile = m_frameProfiles.back();

    ImGui::Begin("Profiler");
    drawHeaderBar();
    ImGui::Separator();
    drawProfileContent();
    ImGui::End();

    m_firstFrame = false;
}

void PerformanceGraphUI::updateProfileData() {
    FrameProfile& frameProfile = m_frameProfiles.emplace_back();

    Profiler::getFrameProfile(frameProfile.threadProfiles);
    frameProfile.numProfiles = 0;
    for (auto it = frameProfile.threadProfiles.begin(); it != frameProfile.threadProfiles.end(); ++it) {
        frameProfile.numProfiles += it->second.size();
        frameProfile.frameStart = it->second[0].startCPU; // std::max(startCPU, startGPU)
        frameProfile.frameEnd = it->second[0].endCPU; // std::max(endCPU, endGPU)
    }

    flushOldFrames();
}

void PerformanceGraphUI::drawHeaderBar() {
    const FrameProfile& frameProfile = m_frameProfiles.back();

    ImGui::BeginGroup();
    {
        ImGui::Checkbox("Pause", &m_profilingPaused);
        ImGui::SameLine(0.0F, 10.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine(0.0F, 10.0F);
        ImGui::Checkbox("Show Idle", &m_showIdleFrameTime);

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
        ImGui::Checkbox("Packed", &m_graphPacked);

        ImGui::SameLine(0.0F, 10.0F);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::SameLine(0.0F, 10.0F);
        if (ImGui::Button("Clear Frames"))
            m_clearFrames = true;

        ImGui::SameLine(0.0F, 10.0F);

        ImGui::Text("%llu frames, %llu profiles across %llu threads\n", m_frameProfiles.size(), frameProfile.numProfiles, frameProfile.threadProfiles.size());
    }
    ImGui::EndGroup();
}

void PerformanceGraphUI::drawProfileContent() {
    const FrameProfile& frameProfile = m_frameProfiles.back();

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
    const FrameProfile& frameProfile = m_frameProfiles.back();

    ImGui::BeginChild("profileTree");
    {
        ImGui::BeginColumns("profileTree", 6, ImGuiColumnsFlags_NoPreserveWidths);
        {
            if (m_firstFrame) { // Initialize column sizes only on first frame
                float x = ImGui::GetWindowContentRegionMax().x;
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
            }

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

            auto it = frameProfile.threadProfiles.find(Application::instance()->getHashedMainThreadId());
            if (it != frameProfile.threadProfiles.end()) {
                const uint64_t& threadId = it->first;
                const ThreadProfile& threadProfile = it->second;

                std::vector<size_t> temp;
                buildProfileTree(threadProfile, ProfileTreeSortOrder::Default, temp);
            }
        }
        ImGui::EndColumns();
    }
    ImGui::EndChild();
}

void PerformanceGraphUI::drawFrameGraph() {
    ImGui::BeginChild("frameGraph");
    uint64_t threadId = Application::instance()->getHashedMainThreadId();

    const float padding = 1.0F;
    const float margin = 1.0F;
    const float segmentSpacing = 0.0F;
    float segmentWidth = 1.0F;

    ImGui::PushItemWidth(-1);
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    const FrameProfile& latestFrame = m_frameProfiles.back();

    ImVec2 bbsize = ImVec2(window->Size.x, window->Size.y);
    ImVec2 bbmin = ImVec2(window->Pos.x, window->Pos.y);
    ImVec2 bbmax = ImVec2(bbmin.x + bbsize.x, bbmin.y + bbsize.y);
    ImVec2 bbminInner = ImVec2(bbmin.x + padding, bbmin.y + padding);
    ImVec2 bbmaxInner = ImVec2(bbmax.x - padding, bbmax.y - padding);
    ImVec2 bbminOuter = ImVec2(bbmin.x - margin, bbmin.y - margin);
    ImVec2 bbmaxOuter = ImVec2(bbmax.x + margin, bbmax.y + margin);

    m_maxFrameProfiles = INT_DIV_CEIL(bbmaxInner.x - bbminInner.x, 100) * 100 + 200;
    float desiredSegmentWidth = (bbmaxInner.x - bbminInner.x) / m_frameProfiles.size();
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
    for (auto it = m_frameProfiles.rbegin(); it != m_frameProfiles.rend(); ++it, ++index) {
        if (index < 0)
            continue;

        x1 = bbmax.x - (index * (segmentWidth + segmentSpacing) + padding);
        x0 = x1 - segmentWidth;

        if (x1 < bbmin.x)
            break;

        auto it1 = it->threadProfiles.find(threadId);
        if (it1 == it->threadProfiles.end())
            continue;
        const ThreadProfile& threadProfiles = it1->second;

        float rootElapsed = (float)Performance::milliseconds(threadProfiles[0].startCPU, threadProfiles[0].endCPU);
        maxFrameTime = glm::max(maxFrameTime, rootElapsed);

        y1 = bbmax.y;
        y0 = y1 - (rootElapsed / m_heightScaleMsec) * (bbsize.y - topPadding);

        drawFrameSegment(threadProfiles, 0, x0, y0, x1, y1);
    }

    const ImU32 frameTimeLineColour = ImGui::ColorConvertFloat4ToU32(ImVec4(0.5F, 0.5F, 0.5F, 0.5F));
    float x, y;
    char str[100] = {'\0'};

    x = bbminInner.x;
    y = bbminInner.y + topPadding;
    sprintf_s(str, sizeof(str), "MAX %02.1f ms (%.1f FPS)", m_heightScaleMsec, 1000.0 / m_heightScaleMsec);
    x += drawFrameTimeOverlay(str, x, y, bbminInner.x, bbminInner.y, bbmaxInner.x, bbmaxInner.y);
    dl->AddLine(ImVec2(x, y), ImVec2(bbmaxInner.x, y), frameTimeLineColour);

    printf("%f %f    %f %f\n", ImGui::GetMousePos().x, ImGui::GetMousePos().y, ImGui::GetCursorPosX(), ImGui::GetCursorPosY());
    x = bbminInner.x;
    y = ImGui::GetMousePos().y;
    if (y > bbmin.y && y < bbmax.y) {
        float msec = (1.0F - ((y - bbmin.y) / (bbmax.y - bbmin.y))) * m_heightScaleMsec;
        sprintf_s(str, sizeof(str), "POS %02.1f ms (%.1f FPS)", msec, 1000.0 / msec);
        x += drawFrameTimeOverlay(str, x, y, bbminInner.x, bbminInner.y, bbmaxInner.x, bbmaxInner.y);
        dl->AddLine(ImVec2(x, y), ImVec2(bbmaxInner.x, y), frameTimeLineColour);
    }

    ImGui::PopClipRect();

    ImGui::EndChild();

    float newHeightScaleMsec = glm::ceil(maxFrameTime / 2.0) * 2.0;
    float adjustmentFactor = 0.05;
    m_heightScaleMsec = m_heightScaleMsec * (1.0F - adjustmentFactor) + newHeightScaleMsec * adjustmentFactor;
}

void PerformanceGraphUI::drawFrameSegment(const ThreadProfile& profiles, const size_t& index, const float& x0, const float& y0, const float& x1, const float& y1) {
    const float h = y1 - y0;
    float y = y1;

    ImU32 colour = ImGui::ColorConvertFloat4ToU32(ImVec4(0.3F, 0.8F, 0.4F, 1.0F));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y), colour);
}

uint32_t PerformanceGraphUI::drawFrameTimeOverlay(const char* str, float x, float y, const float& xmin, const float& ymin, const float& xmax, const float& ymax) {
    ImVec2 size = ImGui::CalcTextSize(str);
    y -= (size.y / 2.0F);

    x = glm::min(glm::max(x, xmin), xmax - size.x);
    y = glm::min(glm::max(y, ymin), ymax - size.y);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + size.x, y + size.y), 0x88000000);
    ImGui::Text("%s", str);
    return size.x;
}

void PerformanceGraphUI::buildProfileTree(const PerformanceGraphUI::ThreadProfile& profiles, const ProfileTreeSortOrder& sortOrder, std::vector<size_t>& tempReorderBuffer, const size_t& currentIndex) {
    double rootElapsedCPU = Performance::milliseconds(profiles[0].startCPU, profiles[0].endCPU);
    double rootElapsedGPU = 0.0;

    const Profiler::Profile& profile = profiles[currentIndex];
    double profileElapsedCPU = Performance::milliseconds(profile.startCPU, profile.endCPU);
    double profileElapsedGPU = 0.0;

    bool hasChildren = profile.lastChildIndex != SIZE_MAX;

    ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren)
        treeNodeFlags |= ImGuiTreeNodeFlags_Leaf;

    bool expanded = ImGui::TreeNodeEx(profile.id->strA, treeNodeFlags);

    ImGui::SameLine();
    ImGui::NextColumn(); // CPU time
    ImGui::Text("%.2f msec", profileElapsedCPU);
    ImGui::NextColumn(); // CPU %
    ImGui::Text("%.2f %%", profileElapsedCPU / rootElapsedCPU * 100.0);
    ImGui::NextColumn(); // GPU time
    ImGui::Text("%.2f msec", profileElapsedGPU);
    ImGui::NextColumn(); // GPU %
    ImGui::Text("%.2f %%", profileElapsedGPU / rootElapsedGPU * 100.0);
    ImGui::NextColumn(); // Colour
//    ImGui::ColorEdit3(profile->name.c_str(), &layer->colour[0], ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs);
    ImGui::NextColumn();

    if (expanded) {
        if (hasChildren) {
            tempReorderBuffer.clear();
            size_t childIndex = currentIndex + 1; // First child is always the next element after its parent
            while (childIndex < profiles.size()) {
                if (sortOrder == ProfileTreeSortOrder::Default) {
                    // Traverse to children right now
                    buildProfileTree(profiles, sortOrder, tempReorderBuffer, childIndex);
                } else {
                    // Store child indices to be traversed after sorting
                    tempReorderBuffer.emplace_back(childIndex);
                }
                childIndex = profiles[childIndex].nextSiblingIndex;
            }

            if (sortOrder != ProfileTreeSortOrder::Default) {
                if (sortOrder == ProfileTreeSortOrder::CpuTimeDescending) {
                    std::sort(tempReorderBuffer.begin(), tempReorderBuffer.end(), [&profiles](const size_t& lhs, const size_t& rhs) {
                        const auto tlhs = profiles[lhs].endCPU - profiles[lhs].startCPU;
                        const auto trhs = profiles[rhs].endCPU - profiles[rhs].startCPU;
                        return tlhs > trhs;
                    });
                }
                if (sortOrder == ProfileTreeSortOrder::CpuTimeAscending) {
                    std::sort(tempReorderBuffer.begin(), tempReorderBuffer.end(), [&profiles](const size_t& lhs, const size_t& rhs) {
                        const auto tlhs = profiles[lhs].endCPU - profiles[lhs].startCPU;
                        const auto trhs = profiles[rhs].endCPU - profiles[rhs].startCPU;
                        return tlhs < trhs;
                    });
                }

                for (const size_t& index : tempReorderBuffer) {
                    buildProfileTree(profiles, sortOrder, tempReorderBuffer, index);
                }
            }
        }
        ImGui::TreePop();
    }
}

void PerformanceGraphUI::flushOldFrames() {
    if (m_frameProfiles.empty())
        return;

//    const FrameProfile& latestFrame = m_frameProfiles.back();
//    double maxFrameAge = 10000; // All frames older than this number of milliseconds are dropped.

    while (true) {
//        const FrameProfile& oldestFrame = m_frameProfiles.front();
//        bool doRemoveFrame = Performance::milliseconds(oldestFrame.frameEnd, latestFrame.frameEnd) > maxFrameAge;

        bool doRemoveFrame = m_frameProfiles.size() > m_maxFrameProfiles;

        if (doRemoveFrame) {
            m_frameProfiles.pop_front();
        } else {
            break;
        }
    }
}
