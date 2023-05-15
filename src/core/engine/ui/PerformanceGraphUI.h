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
        float elapsedMillis = 0.0F;
        size_t nextSiblingIndex = 0;
        uint16_t parentOffset = 0;
//        bool hasChildren = false;
        uint32_t childCount = 0;
    };

    struct FrameProfileData {
        std::vector<ProfileData> profileData;
        size_t rootIndex;
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

    struct ProfileLayer {
        std::string layerName;
        std::vector<uint32_t> pathIndices;
        uint32_t colour;
        bool expanded : 1;
        bool visible : 1;
        bool passedTreeFilter : 1;
    };

    struct LayerInstanceInfo {
        uint32_t layerIndex;
        float accumulatedTotalTime;
        float accumulatedSelfTime;
        float accumulatedTotalTimeAvg;
        float accumulatedSelfTimeAvg;
        float accumulatedTotalTimeSum;
        float accumulatedSelfTimeSum;
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
        bool forceShowRoot = false;
        uint32_t treeDepthLimit = UINT32_MAX;
        std::unordered_map<uint32_t, LayerInstanceInfo> uniqueLayerInstanceInfoMap;
        std::unordered_map<uint32_t, LayerInstanceInfo> pathLayerInstanceInfoMap;
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
    enum ProfileVisibilityMode {
        ProfileVisibilityMode_CPU = 0,
        ProfileVisibilityMode_GPU = 1,
    };
public:
    PerformanceGraphUI();

    ~PerformanceGraphUI() override;

    void update(double dt) override;

    void draw(double dt) override;

private:
    void drawHeaderBar();

    void drawProfileContent(double dt);

    void drawProfileCallStackTree(double dt);

    void drawProfileHotFunctionList(double dt);

    void drawProfileHotFunctionListBody(double dt, const std::vector<ProfileData>& profileData, const std::unordered_map<uint32_t, LayerInstanceInfo>& layerInstanceInfoMap, float lineHeight);

    void drawFrameGraphs(double dt);

    void drawFrameGraph(double dt, const char* strId, const std::vector<FrameProfileData>& frameData, FrameGraphInfo& frameGraphInfo, float x, float y, float w, float h, float padding);

    bool buildFrameSlice(const std::vector<ProfileData>& profileData, size_t index, float y0, float y1, uint32_t treeDepthLimit, FrameGraphSlice& outFrameGraphSlice);

    bool drawFrameSlice(const std::vector<ProfileData>& profileData, size_t index, float x0, float y0, float x1, float y1, uint32_t treeDepthLimit);

    void drawFrameTimeOverlays(const FrameGraphInfo& frameGraphInfo, float xmin, float ymin, float xmax, float ymax);

    float drawFrameTimeOverlayText(const char* str, float x, float y, float xmin, float ymin, float xmax, float ymax);

    void buildProfileTree(std::vector<FrameProfileData>& frameData, FrameGraphInfo& frameGraphInfo, size_t index, const ProfileTreeSortOrder& sortOrder, std::vector<size_t>& tempReorderBuffer);

    void initializeProfileDataTree(FrameProfileData* currentFrame, const Profiler::Profile* profiles, size_t count, size_t stride);

    void initializeProfileTreeLayers(FrameProfileData* currentFrame, const Profiler::Profile* profiles, size_t count, size_t stride, std::vector<uint32_t>& layerPath);

    void compressProfileTreeLayers(FrameProfileData* currentFrame);

    void checkFrameProfileTreeStructureChanged(FrameProfileData* currentFrame, FrameProfileData* previousFrame);

    void updateFrameGraphLayerInstances(const std::vector<ProfileData>& profileData, FrameGraphInfo& frameGraphInfo);

    void updateFrameGraphInfo(FrameGraphInfo& frameGraphInfo, float rootElapsed);

    void updateAccumulatedAverages();

    void flushOldFrames();

    double calculateFrameTimePercentile(const std::vector<double>& frameTimes, std::deque<double>& tempContainer, double percentile);

    uint32_t getUniqueLayerIndex(const std::string& layerName);

    uint32_t getPathLayerIndex(const std::vector<uint32_t>& layerPath);

    bool isGraphNormalized() const;

    bool isProfilePathEqual(const std::vector<uint32_t>& path1, const std::vector<uint32_t>& path2);

    bool isProfilePathDescendent(const std::vector<uint32_t>& path1, const std::vector<uint32_t>& path2);

    size_t getProfileIndexFromPath(const std::vector<ProfileData>& profileData, const std::vector<uint32_t>& path);

    size_t findChildIndexForLayer(const std::vector<ProfileData>& profileData, size_t parentProfileIndex, uint32_t layerIndex);

    size_t findSiblingIndexForLayer(const std::vector<ProfileData>& profileData, size_t firstSiblingProfileIndex, uint32_t layerIndex);

    void updateFilteredLayerPaths(const std::vector<ProfileData>& profileData);

    bool matchSearchTerms(const std::string& str, const std::vector<std::string>& searchTerms);

    bool isProfileHidden(const ProfileData& profile);

    bool hasVisibleChildren(const ProfileData& profile);

    ProfileData* getProfileParent(std::vector<ProfileData>& profiles, size_t index);

    size_t findCommonParent(const std::vector<ProfileData>& profiles, size_t profileIndex1, size_t profileIndex2);

//    void removeChildFromParent(std::vector<ProfileData>& profiles, size_t parentIndex);

    void forEachFrameGraphInfo();

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
    bool m_compressDuplicatedTreePaths;

    GraphVisibilityMode m_graphVisibilityMode;
    ProfileVisibilityMode m_profileVisibilityMode;
    ProfilerDisplayMode m_profilerDisplayMode;

    Performance::moment_t m_lastAverageAccumulationTime;
    Performance::duration_t m_averageAccumulationDuration;
    size_t m_averageAccumulationFrameCount;
    float m_rollingAverageUpdateFactor;
    std::string m_profileNameFilterText;
    std::vector<std::string> m_profileNameFilterSearchTerms;
};


#endif //WORLDENGINE_PERFORMANCEGRAPHUI_H
