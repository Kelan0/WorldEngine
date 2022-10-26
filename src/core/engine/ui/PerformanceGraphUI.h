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
        uint16_t parentOffset = 0;
        bool hasChildren = false;
    };

    struct FrameProfileData {
        std::vector<ProfileData> profileData;
        bool treeStructureChanged;
//        size_t treeStructureHash;
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
        float frameTimePercentile999;
        float frameTimePercentile99;
        float frameTimePercentile90;
        float allFramesTimeAvg;
        float frameTimeAvg;
        float frameTimeSum;
//        double frameTimeRollingAvg;
//        double frameTimeRollingAvgDecayRate = 0.01;
        uint32_t frameSequence = 0;
        uint32_t maxFrameProfiles = 500;
//        size_t frameGraphRootIndex = 0;
        std::vector<uint32_t> frameGraphRootPath;
        float heightScaleMsec = 5.0F;
    };

    struct ProfileLayer {
        std::string layerName;
        std::vector<uint32_t> pathIndices;
        uint32_t colour;
        float accumulatedTotalTime;
        float accumulatedSelfTime;
        float accumulatedTotalTimeAvg;
        float accumulatedSelfTimeAvg;
        float accumulatedTotalTimeSum;
        float accumulatedSelfTimeSum;
        bool expanded : 1;
        bool visible : 1;
        bool passedTreeFilter : 1;
    };

    enum ProfileTreeSortOrder {
        ProfileTreeSortOrder_Default = 0,
        ProfileTreeSortOrder_CpuTimeAscending = 1,
        ProfileTreeSortOrder_CpuTimeDescending = 2,
    };

    enum ProfilerDisplayMode {
        ProfilerDisplayMode_CallStack = 0,
        ProfilerDisplayMode_HotFunctionList = 1,
        ProfilerDisplayMode_HotPathList = 2,
    };
    enum GraphVisibilityMode {
        GraphVisibilityMode_Both = 0,
        GraphVisibilityMode_CPU = 1,
        GraphVisibilityMode_GPU = 2,
    };
public:
    PerformanceGraphUI();

    ~PerformanceGraphUI() override;

    void update(const double& dt) override;

    void draw(const double& dt) override;

private:
    void drawHeaderBar();

    void drawProfileContent(const double& dt);

    void drawProfileCallStackTree(const double& dt);

    void drawProfileHotFunctionList(const double& dt);

    void drawFrameGraphs(const double& dt);

    void drawFrameGraph(const double& dt, const char* strId, const std::vector<FrameProfileData>& frameData, FrameGraphInfo& frameGraphInfo, const float& x, const float& y, const float& w, const float& h, const float& padding);

    bool drawFrameSlice(const std::vector<ProfileData>& profileData, const size_t& index, const float& x0, const float& y0, const float& x1, const float& y1, const uint32_t& treeDepthLimit);

    void drawFrameTimeOverlays(const FrameGraphInfo& frameGraphInfo, const float& xmin, const float& ymin, const float& xmax, const float& ymax);

    float drawFrameTimeOverlayText(const char* str, float x, float y, const float& xmin, const float& ymin, const float& xmax, const float& ymax);

    void buildProfileTree(const FrameProfileData& frameData, FrameGraphInfo& frameGraphInfo, const size_t& index, const ProfileTreeSortOrder& sortOrder, std::vector<size_t>& tempReorderBuffer);

    void initializeProfileDataTree(FrameProfileData* currentFrame, const Profiler::Profile* profiles, const size_t& count, const size_t& stride);

    void initializeProfileTreeLayers(FrameProfileData* currentFrame, const Profiler::Profile* profiles, const size_t& count, const size_t& stride, std::vector<uint32_t>& layerPath);

    void checkFrameProfileTreeStructureChanged(FrameProfileData* currentFrame, FrameProfileData* previousFrame);

    void updateFrameGraphInfo(FrameGraphInfo& frameGraphInfo, const float& rootElapsed);

    void updateAccumulatedAverages();

    void flushOldFrames();

    double calculateFrameTimePercentile(const std::vector<double>& frameTimes, std::deque<double>& tempContainer, double percentile);

    const uint32_t& getUniqueLayerIndex(const std::string& layerName);

    const uint32_t& getPathLayerIndex(const std::vector<uint32_t>& layerPath);

    bool isGraphNormalized() const;

    bool isProfilePathEqual(const std::vector<uint32_t>& path1, const std::vector<uint32_t>& path2);

    size_t getProfileIndexFromPath(const std::vector<ProfileData>& profileData, const std::vector<uint32_t>& path);

    size_t findChildIndexForLayer(const std::vector<ProfileData>& profileData, const size_t& parentProfileIndex, const uint32_t& layerIndex);

    void updateFilteredLayerPaths(const std::vector<ProfileData>& profileData);

    bool matchSearchTerms(const std::string& str, const std::vector<std::string>& searchTerms);

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

    GraphVisibilityMode m_graphVisibilityMode;
    ProfilerDisplayMode m_profilerDisplayMode;

    Performance::moment_t m_lastAverageAccumulationTime;
    Performance::duration_t m_averageAccumulationDuration;
    size_t m_averageAccumulationFrameCount;
    float m_rollingAverageUpdateFactor;
    std::string m_profileNameFilterText;
    std::vector<std::string> m_profileNameFilterSearchTerms;
};


#endif //WORLDENGINE_PERFORMANCEGRAPHUI_H
