#ifndef WORLDENGINE_PERFORMANCEGRAPHUI_H
#define WORLDENGINE_PERFORMANCEGRAPHUI_H

#include "core/core.h"
#include "core/engine/ui/UI.h"

class PerformanceGraphUI : public UI {
    typedef std::vector<Profiler::Profile> ThreadProfile; // List of profiles for one thread

    struct FrameProfile {
        std::unordered_map<uint64_t, ThreadProfile> threadProfiles; // Map of threads to thread profiles for one frame
        Performance::moment_t frameStart;
        Performance::moment_t frameEnd;
        size_t numProfiles;
    };

    enum class ProfileTreeSortOrder {
        Default = 0,
        CpuTimeAscending = 1,
        CpuTimeDescending = 2,
    };
public:
    PerformanceGraphUI();

    ~PerformanceGraphUI() override;

    void draw(double dt) override;

private:
    void updateProfileData();

    void drawHeaderBar();

    void drawProfileContent();

    void drawProfileTree();

    void drawFrameGraph();

    void drawFrameSegment(const ThreadProfile& profiles, const size_t& index, const float& x0, const float& y0, const float& x1, const float& y1);

    uint32_t drawFrameTimeOverlay(const char* str, float x, float y, const float& xmin, const float& ymin, const float& xmax, const float& ymax);

    void buildProfileTree(const ThreadProfile& profiles, const ProfileTreeSortOrder& sortOrder, std::vector<size_t>& tempReorderBuffer, const size_t& currentIndex = 0);

    void flushOldFrames();

private:
    std::deque<FrameProfile> m_frameProfiles;
    size_t m_maxFrameProfiles;

    bool m_firstFrame;
    bool m_profilingPaused;
    bool m_showIdleFrameTime;
    bool m_graphsNormalized;
    bool m_graphPacked;
    bool m_clearFrames;
    int m_graphVisible;
    float m_heightScaleMsec;
};


#endif //WORLDENGINE_PERFORMANCEGRAPHUI_H
