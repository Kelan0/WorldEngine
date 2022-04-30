cmake_minimum_required(VERSION 3.2)
project(WorldEngine)

set(CMAKE_CXX_STANDARD 20)

set(SDL2_PATH "D:/Code/Libraries/SDL2-2.0.20")
set(VULKAN_PATH "D:/Code/VulkanSDK/1.2.198.1")
set(GLM_PATH "D:/Code/Libraries/glm-0.9.9.8")
set(ENTT_PATH "D:/Code/Libraries/entt")
set(STB_IMAGE_PATH "D:/Code/Libraries/stb")
#set(VOLK_PATH "D:/Code/Libraries/volk")
set(ITT_PATH "C:/Program Files (x86)/Intel/oneAPI/vtune/2022.2.0/sdk")

#IF(CMAKE_COMPILER_IS_GNUCC)
#    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O1")
#    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O1")
#ENDIF(CMAKE_COMPILER_IS_GNUCC)

if (MSVC)
set_target_properties(${BUILD_TARGET} PROPERTIES LINK_FLAGS "/PROFILE")
endif()

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
add_compile_definitions(_DEBUG)
endif()

# INIT SDL2 LIBRARY
set(SDL2_INCLUDE_DIR "${SDL2_PATH}/include")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SDL2_LIB_DIR "${SDL2_PATH}/lib/x64")
else()
    set(SDL2_LIB_DIR "${SDL2_PATH}/lib/x86")
endif()
list(APPEND COPY_BINARIES "${SDL2_LIB_DIR}/SDL2.dll")
set(SDL2_LIBRARIES SDL2main SDL2)


# INIT VULKAN LIBRARY
set(ENV{VULKAN_SDK} ${VULKAN_PATH})
find_package(Vulkan REQUIRED)
#set(Vulkan_INCLUDE_DIR "${VULKAN_PATH}/Include")
#if(CMAKE_SIZEOF_VOID_P EQUAL 8)
#    set(VULKAN_LIB_DIR "${VULKAN_PATH}/Lib")
#else()
#    set(VULKAN_LIB_DIR "${VULKAN_PATH}/Lib32")
#endif()


# INIT GLM LIBRARY
set(GLM_INCLUDE_DIR "${GLM_PATH}")


# INIT ENTT LIBRARY
set(ENTT_INCLUDE_DIR "${ENTT_PATH}")


# INIT STB_IMAGE LIBRARY
set(STB_IMAGE_INCLUDE_DIR "${STB_IMAGE_PATH}")
add_library(stb_image "src/core/stb_image_impl.cpp")
set(STB_IMAGE_LIBRARIES stb_image)


## INIT VOLK LIBRARY
#set(VOLK_INCLUDE_DIR "${VOLK_PATH}")
#add_library(volk "src/core/volk_impl.cpp")
#set(VOLK_LIBRARIES volk)



# INIT ITT LIBRARY
set(ITT_INCLUDE_DIR "${ITT_PATH}/include")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ITT_LIB_DIR "${ITT_PATH}/lib64")
else()
    set(ITT_LIB_DIR "${ITT_PATH}/lib32")
endif()
set(ITT_LIBRARIES libittnotify.lib)





include_directories(
        ${PROJECT_SOURCE_DIR}/src
        ${SDL2_INCLUDE_DIR}
        ${Vulkan_INCLUDE_DIR}
        ${GLM_INCLUDE_DIR}
        ${ENTT_INCLUDE_DIR}
        ${STB_IMAGE_INCLUDE_DIR}
#        ${VOLK_INCLUDE_DIR}
        ${ITT_INCLUDE_DIR})

link_directories(
        ${SDL2_LIB_DIR}
        ${ITT_LIB_DIR})

file(GLOB_RECURSE PROJECT_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.c
        ${CMAKE_SOURCE_DIR}/src/*.hpp
        ${CMAKE_SOURCE_DIR}/src/*.h)

add_executable(${PROJECT_NAME} src/main.cpp src/core/core.h src/core/hash.h src/core/application/Application.cpp src/core/application/Application.h src/core/application/InputHandler.cpp src/core/application/InputHandler.h src/core/engine/geometry/MeshData.cpp src/core/engine/geometry/MeshData.h src/core/engine/renderer/RenderCamera.cpp src/core/engine/renderer/RenderCamera.h src/core/engine/renderer/RenderComponent.cpp src/core/engine/renderer/RenderComponent.h src/core/engine/renderer/SceneRenderer.cpp src/core/engine/renderer/SceneRenderer.h src/core/engine/scene/Camera.cpp src/core/engine/scene/Camera.h src/core/engine/scene/Entity.cpp src/core/engine/scene/Entity.h src/core/engine/scene/EntityHierarchy.cpp src/core/engine/scene/EntityHierarchy.h src/core/engine/scene/Scene.cpp src/core/engine/scene/Scene.h src/core/engine/scene/Transform.cpp src/core/engine/scene/Transform.h src/core/engine/scene/event/EventDispatcher.cpp src/core/engine/scene/event/EventDispatcher.h src/core/engine/scene/event/Events.h src/core/graphics/Buffer.cpp src/core/graphics/Buffer.h src/core/graphics/CommandPool.cpp src/core/graphics/CommandPool.h src/core/graphics/DescriptorSet.cpp src/core/graphics/DescriptorSet.h src/core/graphics/DeviceMemory.cpp src/core/graphics/DeviceMemory.h src/core/graphics/FrameResource.cpp src/core/graphics/FrameResource.h src/core/graphics/GraphicsManager.cpp src/core/graphics/GraphicsManager.h src/core/graphics/GraphicsPipeline.cpp src/core/graphics/GraphicsPipeline.h src/core/graphics/Image.cpp src/core/graphics/Image.h src/core/graphics/Mesh.cpp src/core/graphics/Mesh.h src/core/graphics/Texture.cpp src/core/graphics/Texture.h src/core/util/DebugUtils.cpp src/core/util/DebugUtils.h src/core/util/Exception.cpp src/core/util/Exception.h src/core/util/Util.cpp src/core/util/Util.h src/core/util/Profiler.cpp src/core/util/Profiler.h src/core/thread/Scheduler.cpp src/core/thread/Scheduler.h src/core/thread/ThreadPool.cpp src/core/thread/ThreadPool.h src/core/thread/Task.cpp src/core/thread/Task.h src/core/thread/ThreadUtils.cpp src/core/thread/ThreadUtils.h src/core/engine/renderer/RenderProperties.cpp src/core/engine/renderer/RenderProperties.h src/core/util/DenseFlagArray.cpp src/core/util/DenseFlagArray.h src/core/util/EntityChangeTracker.cpp src/core/util/EntityChangeTracker.h src/core/engine/scene/bound/Frustum.cpp src/core/engine/scene/bound/Frustum.h src/core/engine/scene/bound/Plane.cpp src/core/engine/scene/bound/Plane.h src/core/engine/scene/bound/Sphere.cpp src/core/engine/scene/bound/Sphere.h src/core/engine/scene/bound/Intersection.cpp src/core/engine/scene/bound/Intersection.h src/core/engine/renderer/ImmediateRenderer.cpp src/core/engine/renderer/ImmediateRenderer.h src/core/graphics/RenderPass.cpp src/core/graphics/RenderPass.h src/core/Vulkan.cpp src/core/Vulkan.h src/core/graphics/Framebuffer.cpp src/core/graphics/Framebuffer.h src/core/engine/renderer/renderPasses/DeferredRenderer.cpp src/core/engine/renderer/renderPasses/DeferredRenderer.h)

target_link_libraries(${PROJECT_NAME}
        ${Vulkan_LIBRARIES}
        ${SDL2_LIBRARIES}
        ${STB_IMAGE_LIBRARIES}
#        ${VOLK_LIBRARIES}
        ${ITT_LIBRARIES})


if(WIN32)
    foreach (CPY_SRC ${COPY_BINARIES})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "${CPY_SRC}" "${CMAKE_BINARY_DIR}")
    endforeach ()

    #configure_file(${SDL2_LIBRARY_DLLS} ${CMAKE_BINARY_DIR})
endif()


add_custom_target(copy_resources ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res" "${PROJECT_BINARY_DIR}/res"
        COMMENT Copying resources)

add_dependencies(${PROJECT_NAME} copy_resources)