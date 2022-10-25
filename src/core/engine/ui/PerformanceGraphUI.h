#ifndef WORLDENGINE_PERFORMANCEGRAPHUI_H
#define WORLDENGINE_PERFORMANCEGRAPHUI_H

#include "core/core.h"
#include "core/engine/ui/UI.h"
#include "core/util/Profiler.h"

class PerformanceGraphUI : public UI {
    typedef Profiler::CPUProfile ThreadProfile;
    typedef Profiler::GPUProfile GPUProfile;
    typedef std::vector<ThreadProfile> ThreadProfiles; // List of profiles for one thread
    typedef std::vector<GPUProfile> GpuProfiles; // List of profiles for one thread
//    typedef std::vector<size_t> ProfileCallPath;

    struct ProfileData {
        uint32_t layerIndex = UINT32_MAX;
        uint32_t pathIndex = UINT32_MAX;
        float elapsedMillis;
        size_t nextSiblingIndex = 0;
        bool hasChildren = false;
    };

    struct FrameProfileData {
        std::vector<ProfileData> profileData;
    };

    struct FrameGraphSegment {
        float height;
        uint32_t colour;
    };

    struct FrameGraphSlice {
        std::vector<FrameGraphSegment> segments;
    };

    struct FrameGraphInfo {
        std::vector<float> frameTimes;
        std::vector<std::pair<float, uint32_t>> sortedFrameTimes;
        double frameTimePercentile999;
        double frameTimePercentile99;
        double frameTimePercentile90;
        double frameTimeAvg;
        double frameTimeRollingAvg;
        double frameTimeRollingAvgDecayRate = 0.01;
        uint32_t frameSequence = 0;
        uint32_t maxFrameProfiles = 500;
        size_t frameGraphRootIndex = 0;
        float heightScaleMsec = 5.0F;
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

    void update(const double& dt) override;

    void draw(const double& dt) override;

private:
    void drawHeaderBar();

    void drawProfileContent(const double& dt);

    void drawProfileTree(const double& dt);

    void drawFrameGraphs(const double& dt);

    void drawFrameGraph(const double& dt, const char* strId, const std::vector<FrameProfileData>& frameData, FrameGraphInfo& frameGraphInfo, const float& x, const float& y, const float& w, const float& h, const float& padding);

    bool drawFrameSlice(const std::vector<ProfileData>& profileData, const size_t& index, const float& x0, const float& y0, const float& x1, const float& y1);

    void drawFrameTimeOverlays(const FrameGraphInfo& frameGraphInfo, const float& xmin, const float& ymin, const float& xmax, const float& ymax);

    float drawFrameTimeOverlayText(const char* str, float x, float y, const float& xmin, const float& ymin, const float& xmax, const float& ymax);

    void buildProfileTree(const FrameProfileData& frameData, FrameGraphInfo& frameGraphInfo, const size_t& index, const ProfileTreeSortOrder& sortOrder, std::vector<size_t>& tempReorderBuffer);

    void updateFrameGraphInfo(FrameGraphInfo& frameGraphInfo, const float& rootElapsed);

    void flushOldFrames();

    double calculateFrameTimePercentile(const std::vector<double>& frameTimes, std::deque<double>& tempContainer, double percentile);

    const uint32_t& getUniqueLayerIndex(const std::string& layerName);

    const uint32_t& getPathLayerIndex(const std::vector<uint32_t>& layerPath);

private:
    size_t m_maxFrameProfiles;

    std::unordered_map<uint64_t, FrameGraphInfo> m_threadFrameGraphInfo;
    std::vector<ProfileLayer> m_layers;
    std::unordered_map<std::string, uint32_t> m_uniqueLayerIndexMap;
    std::unordered_map<std::vector<uint32_t>, uint32_t> m_pathLayerIndexMap;

    FrameGraphInfo m_gpuFrameGraphInfo;
    GpuProfiles m_currentGpuProfiles;
    std::vector<FrameProfileData> m_gpuFrameProfileData;
    std::unordered_map<uint64_t, ThreadProfiles> m_currentThreadProfiles;
    std::unordered_map<uint64_t, std::vector<FrameProfileData>> m_threadFrameProfileData;

    bool m_profilingPaused;
    bool m_graphsNormalized;
    bool m_showFrameTime90Percentile;
    bool m_showFrameTime99Percentile;
    bool m_showFrameTime999Percentile;
    bool m_clearFrames;
    int m_graphVisible;

};


#endif //WORLDENGINE_PERFORMANCEGRAPHUI_H
