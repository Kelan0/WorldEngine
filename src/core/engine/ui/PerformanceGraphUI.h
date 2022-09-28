#ifndef WORLDENGINE_PERFORMANCEGRAPHUI_H
#define WORLDENGINE_PERFORMANCEGRAPHUI_H

#include "core/core.h"
#include "core/engine/ui/UI.h"

class PerformanceGraphUI : public UI {
    typedef std::vector<Profiler::Profile> ThreadProfile; // List of profiles for one thread
//    typedef std::vector<size_t> ProfileCallPath;

    struct ProfileData {
        uint32_t layerIndex = UINT32_MAX;
        float elapsedCPU;
        uint32_t parentIndex = 0;
        uint32_t nextSiblingIndex = 0;
        bool hasChildren = false;
    };

    struct FrameProfileData {
        std::vector<ProfileData> profileData;
    };

    struct FrameGraphSegment {
        float y0;
        float y1;
        uint32_t layerIndex;
    };

    struct FrameGraphSlice {
        std::vector<FrameGraphSegment> segments;
    };

    struct ThreadInfo {
        std::vector<double> frameTimes;
        std::vector<double> sortedFrameTimes;
        double frameTimePercentile999;
        double frameTimePercentile99;
        double frameTimePercentile90;
        double frameTimeAvg;
    };

    struct ProfileLayer {
        std::string layerName;
        float colour[3];
        bool expanded;
        bool visible;
    };

    enum class ProfileTreeSortOrder {
        Default = 0,
        CpuTimeAscending = 1,
        CpuTimeDescending = 2,
    };
public:
    PerformanceGraphUI();

    ~PerformanceGraphUI() override;

    void update(double dt) override;

    void draw(double dt) override;

private:
    void drawHeaderBar();

    void drawProfileContent();

    void drawProfileTree();

    void drawFrameGraph();

    void buildFrameSlice(const std::vector<ProfileData>& profileData, const size_t& index, const float& y0, const float& y1, std::vector<FrameGraphSlice>& outSlice);

    void drawFrameSlice(const std::vector<ProfileData>& profileData, const size_t& index, const float& x0, const float& y0, const float& x1, const float& y1);

    void drawFrameTimeOverlays(const uint64_t& threadId, const float& xmin, const float& ymin, const float& xmax, const float& ymax);

    float drawFrameTimeOverlayText(const char* str, float x, float y, const float& xmin, const float& ymin, const float& xmax, const float& ymax);

    void buildProfileTree(const std::vector<ProfileData>& profiles, const size_t& index, const ProfileTreeSortOrder& sortOrder, std::vector<size_t>& tempReorderBuffer);

    void flushOldFrames();

    double calculateFrameTimePercentile(const std::vector<double>& frameTimes, std::deque<double>& tempContainer, double percentile);

    const uint32_t& getUniqueLayerIndex(const std::string& layerName);

    //const uint32_t& getPathLayerIndex(const std::vector<uint32_t>& layerPath);

private:
    size_t m_maxFrameProfiles;

    std::unordered_map<uint64_t, ThreadInfo> m_threadInfo;
    std::vector<ProfileLayer> m_layers;
    std::unordered_map<std::string, uint32_t> m_uniqueLayerIndexMap;
    //std::unordered_map<std::vector<uint32_t>, uint32_t> m_pathLayerIndexMap;

    std::unordered_map<uint64_t, std::vector<Profiler::Profile>> m_currentFrameProfile;
    std::unordered_map<uint64_t, std::vector<FrameProfileData>> m_frameProfileData;

    bool m_firstFrame;
    bool m_profilingPaused;
    bool m_graphsNormalized;
    bool m_showFrameTime90Percentile;
    bool m_showFrameTime99Percentile;
    bool m_showFrameTime999Percentile;
    bool m_clearFrames;
    int m_graphVisible;
    float m_heightScaleMsec;

};


#endif //WORLDENGINE_PERFORMANCEGRAPHUI_H
