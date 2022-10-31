cmake_minimum_required(VERSION 3.2)
project(WorldEngine)

set(CMAKE_CXX_STANDARD 20)

#set(SDL2_DIR "C:/Users/Kelan/Library/SDL/SDL2-2.24.1/cmake")
#set(VULKAN_PATH "D:/Code/VulkanSDK/1.2.198.1")
#set(GLM_PATH "D:/Code/Libraries/glm-0.9.9.8")
#set(STB_IMAGE_PATH "D:/Code/Libraries/stb")
##set(VOLK_PATH "D:/Code/Libraries/volk")
#set(ITT_PATH "C:/Program Files (x86)/Intel/oneAPI/vtune/2022.2.0/sdk")

#set(SDL2_INCLUDE_DIRS "${SDL2_PATH}/include")

set(CREATE_RESOURCE_SYMLINK 1)

set(VULKAN_DIR "C:\\VulkanSDK\\1.2.198.1")

if ("$ENV{VULKAN_SDK}" STREQUAL "")
    message(FATAL_ERROR "VULKAN_SDK environment variable is not defined. Please install the Vulkan SDK")
elseif(NOT EXISTS "$ENV{VULKAN_SDK}")
    message(FATAL_ERROR "VULKAN_SDK environment variable path does not exist: $ENV{VULKAN_SDK}")
else()
    message(STATUS "Found Vulkan SDK at: $ENV{VULKAN_SDK}")
endif()

if ("${SDL2_DIR}" STREQUAL "")
    message(FATAL_ERROR "Please define the variable SDL2_DIR to point to the location of the folder containing FindSDL2.cmake or equivalent")
elseif(NOT EXISTS "${SDL2_DIR}")
    message(FATAL_ERROR "SDL2_DIR path does not exist: ${SDL2_DIR}")
endif()

if ("${glm_DIR}" STREQUAL "")
    message(FATAL_ERROR "Please define the variable glm_DIR to point to the location of the folder containing Findglm.cmake or equivalent")
elseif(NOT EXISTS "${glm_DIR}")
    message(FATAL_ERROR "glm_DIR path does not exist: ${glm_DIR}")
endif()

if ("${VTUNE_DIR}" STREQUAL "")
    message(WARNING "VTUNE_DIR variable is not defined. Intel Instrumentation and Tracing Technology API will be disabled, no profile data will be produced for VTune Profiler")
elseif(NOT EXISTS "${VTUNE_DIR}")
    message(FATAL_ERROR "VTUNE_DIR was defined but the file path '${VTUNE_DIR}' does not exist.")
else()
    message(STATUS "Found Intel VTune API at ${VTUNE_DIR}")
endif()

find_package(Vulkan REQUIRED FATAL_ERROR)
if ("${Vulkan_VERSION}" VERSION_GREATER_EQUAL "1.3")
    add_compile_definitions(USE_VULKAN_HASH_HPP=1)
else()
    add_compile_definitions(USE_VULKAN_HASH_HPP=0)
endif()

find_package(SDL2 REQUIRED)
if (${SDL2_SDL2_FOUND}) # Somebody messed up naming for MinGW SDL2. SDL2_FOUND is never set to true, instead, SDL2_SDL2_FOUND is, but find_package will not check this automatically
    message(STATUS "The above error is fine, please ignore it")
elseif(NOT ${SDL2_FOUND})
    message(FATAL_ERROR "SDL2 not found")
endif()

find_package(glm REQUIRED FATAL_ERROR)

#IF(CMAKE_COMPILER_IS_GNUCC)
#    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O1")
#    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O1")
#ENDIF(CMAKE_COMPILER_IS_GNUCC)

if (MSVC)
    set_target_properties(${BUILD_TARGET} PROPERTIES LINK_FLAGS "/PROFILE")
endif ()

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    add_compile_definitions(_DEBUG)
endif ()

# INIT SDL2 LIBRARY
if (WIN32)
    list(APPEND COPY_BINARIES "${SDL2_BINDIR}/SDL2.dll")
#    set(SDL2_LIBRARIES SDL2main SDL2)
endif()

set(ENTT_INCLUDE_DIRS "src/extern/entt/")
set(STB_IMAGE_INCLUDE_DIRS "src/extern/stbi/")
add_library(stb_image "src/extern/stbi/stb_image_impl.cpp")
set(STB_IMAGE_LIBRARIES stb_image)

# INIT ITT LIBRARY
if (EXISTS "${VTUNE_DIR}")
    set(ITT_INCLUDE_DIRS "${VTUNE_DIR}/include")
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ITT_LIB_DIR "${VTUNE_DIR}/lib64")
    else()
        set(ITT_LIB_DIR "${VTUNE_DIR}/lib32")
    endif()

    if (WIN32)
        set(ITT_LIBRARIES libittnotify.lib)
    else()
        set(ITT_LIBRARIES -littnotify)
    endif()
    add_compile_definitions(ITT_ENABLED=1)
else()
    add_compile_definitions(ITT_ENABLED=0)
endif()

include_directories(
        ${PROJECT_SOURCE_DIR}/src
        ${SDL2_INCLUDE_DIRS}
        ${Vulkan_INCLUDE_DIRS}
        ${GLM_INCLUDE_DIRS}
        ${ENTT_INCLUDE_DIRS}
        #        ${IMGUI_INCLUDE_DIR}
        ${STB_IMAGE_INCLUDE_DIRS}
        #        ${VOLK_INCLUDE_DIR}
        ${ITT_INCLUDE_DIRS})


link_directories(
        ${SDL2_LIBDIR}
        ${ITT_LIB_DIR})

#file(GLOB_RECURSE PROJECT_SOURCE_FILES
#        ${CMAKE_SOURCE_DIR}/src/*.cpp
#        ${CMAKE_SOURCE_DIR}/src/*.c
#        ${CMAKE_SOURCE_DIR}/src/*.hpp
#        ${CMAKE_SOURCE_DIR}/src/*.h)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/core/core.h
        src/core/hash.h
        src/core/application/Application.cpp
        src/core/application/Application.h
        src/core/application/InputHandler.cpp
        src/core/application/InputHandler.h
        src/core/engine/geometry/MeshData.cpp
        src/core/engine/geometry/MeshData.h
        src/core/engine/renderer/RenderCamera.cpp
        src/core/engine/renderer/RenderCamera.h
        src/core/engine/renderer/RenderComponent.cpp
        src/core/engine/renderer/RenderComponent.h
        src/core/engine/renderer/SceneRenderer.cpp
        src/core/engine/renderer/SceneRenderer.h
        src/core/engine/scene/Camera.cpp
        src/core/engine/scene/Camera.h
        src/core/engine/scene/Entity.cpp
        src/core/engine/scene/Entity.h
        src/core/engine/scene/EntityHierarchy.cpp
        src/core/engine/scene/EntityHierarchy.h
        src/core/engine/scene/Scene.cpp
        src/core/engine/scene/Scene.h
        src/core/engine/scene/Transform.cpp
        src/core/engine/scene/Transform.h
        src/core/engine/event/EventDispatcher.cpp
        src/core/engine/event/EventDispatcher.h
        src/core/graphics/Buffer.cpp
        src/core/graphics/Buffer.h
        src/core/graphics/CommandPool.cpp
        src/core/graphics/CommandPool.h
        src/core/graphics/DescriptorSet.cpp
        src/core/graphics/DescriptorSet.h
        src/core/graphics/DeviceMemory.cpp
        src/core/graphics/DeviceMemory.h
        src/core/graphics/FrameResource.cpp
        src/core/graphics/FrameResource.h
        src/core/graphics/GraphicsManager.cpp
        src/core/graphics/GraphicsManager.h
        src/core/graphics/GraphicsPipeline.cpp
        src/core/graphics/GraphicsPipeline.h
        src/core/graphics/Image2D.cpp
        src/core/graphics/Image2D.h
        src/core/graphics/ImageCube.cpp
        src/core/graphics/ImageCube.h
        src/core/graphics/ImageData.cpp
        src/core/graphics/ImageData.h
        src/core/graphics/Mesh.cpp
        src/core/graphics/Mesh.h
        src/core/graphics/Texture.cpp
        src/core/graphics/Texture.h
        src/core/util/DebugUtils.cpp
        src/core/util/DebugUtils.h
        src/core/util/Exception.cpp
        src/core/util/Exception.h
        src/core/util/Util.cpp
        src/core/util/Util.h
        src/core/util/Profiler.cpp
        src/core/util/Profiler.h
        src/core/thread/ThreadPool.cpp
        src/core/thread/ThreadPool.h
        src/core/thread/Task.cpp
        src/core/thread/Task.h
        src/core/thread/ThreadUtils.cpp
        src/core/thread/ThreadUtils.h
        src/core/engine/renderer/RenderProperties.cpp
        src/core/engine/renderer/RenderProperties.h
        src/core/util/DenseFlagArray.cpp
        src/core/util/DenseFlagArray.h
        src/core/util/EntityChangeTracker.cpp
        src/core/util/EntityChangeTracker.h
        src/core/engine/scene/bound/Frustum.cpp
        src/core/engine/scene/bound/Frustum.h
        src/core/engine/scene/bound/Plane.cpp
        src/core/engine/scene/bound/Plane.h
        src/core/engine/scene/bound/Sphere.cpp
        src/core/engine/scene/bound/Sphere.h
        src/core/engine/scene/bound/Intersection.cpp
        src/core/engine/scene/bound/Intersection.h
        src/core/engine/renderer/ImmediateRenderer.cpp
        src/core/engine/renderer/ImmediateRenderer.h
        src/core/graphics/RenderPass.cpp
        src/core/graphics/RenderPass.h
        src/core/Vulkan.cpp
        src/core/Vulkan.h
        src/core/graphics/Framebuffer.cpp
        src/core/graphics/Framebuffer.h
        src/core/engine/renderer/renderPasses/DeferredRenderer.cpp
        src/core/engine/renderer/renderPasses/DeferredRenderer.h
        src/core/engine/renderer/Material.cpp
        src/core/engine/renderer/Material.h
        src/core/graphics/ComputePipeline.cpp
        src/core/graphics/ComputePipeline.h
        src/core/graphics/ShaderUtils.cpp
        src/core/graphics/ShaderUtils.h
        src/core/graphics/BufferView.cpp
        src/core/graphics/BufferView.h
        src/core/graphics/ImageView.cpp
        src/core/graphics/ImageView.h
        src/core/engine/renderer/EnvironmentMap.cpp
        src/core/engine/renderer/EnvironmentMap.h
        src/core/engine/renderer/ShadowMap.cpp
        src/core/engine/renderer/ShadowMap.h
        src/core/application/Engine.cpp
        src/core/application/Engine.h
        src/core/engine/renderer/renderPasses/LightRenderer.cpp
        src/core/engine/renderer/renderPasses/LightRenderer.h
        src/core/engine/renderer/RenderLight.cpp
        src/core/engine/renderer/RenderLight.h
        src/core/engine/renderer/LightComponent.cpp
        src/core/engine/renderer/LightComponent.h
        src/core/util/PlatformUtils.cpp
        src/core/util/PlatformUtils.h
        src/extern/imgui/imconfig.h
        src/extern/imgui/imgui.cpp
        src/extern/imgui/imgui.h
        src/extern/imgui/imgui_draw.cpp
        src/extern/imgui/imgui_internal.h
        src/extern/imgui/imgui_tables.cpp
        src/extern/imgui/imgui_widgets.cpp
        src/extern/imgui/imstb_rectpack.h
        src/extern/imgui/imstb_textedit.h
        src/extern/imgui/imstb_truetype.h
        src/extern/imgui/imgui_impl_sdl.cpp
        src/extern/imgui/imgui_impl_sdl.h
        src/extern/imgui/imgui_impl_vulkan.cpp
        src/extern/imgui/imgui_impl_vulkan.h
        src/extern/imgui/imgui_stdlib.cpp
        src/extern/imgui/imgui_stdlib.h
        src/extern/imgui/imgui_demo.cpp
        src/core/engine/renderer/renderPasses/UIRenderer.cpp
        src/core/engine/renderer/renderPasses/UIRenderer.h
        src/core/engine/ui/PerformanceGraphUI.cpp
        src/core/engine/ui/PerformanceGraphUI.h
        src/core/engine/ui/UI.cpp
        src/core/engine/ui/UI.h
        src/core/engine/renderer/renderPasses/PostProcessRenderer.cpp
        src/core/engine/renderer/renderPasses/PostProcessRenderer.h
        src/core/engine/renderer/renderPasses/ReprojectionRenderer.cpp
        src/core/engine/renderer/renderPasses/ReprojectionRenderer.h
        src/core/engine/event/GraphicsEvents.cpp
        src/core/engine/event/GraphicsEvents.h
        src/core/engine/event/ApplicationEvents.cpp
        src/core/engine/event/ApplicationEvents.h
        src/core/graphics/GraphicsResource.cpp
        src/core/graphics/GraphicsResource.h)

target_link_libraries(${PROJECT_NAME}
        ${Vulkan_LIBRARIES}
        ${SDL2_LIBRARIES}
        ${STB_IMAGE_LIBRARIES}
        #        ${VOLK_LIBRARIES}
        ${ITT_LIBRARIES})

if (WIN32)
    foreach (CPY_SRC ${COPY_BINARIES})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "${CPY_SRC}" "${CMAKE_BINARY_DIR}")
    endforeach ()

    #configure_file(${SDL2_LIBRARY_DLLS} ${CMAKE_BINARY_DIR})
endif ()

file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/res" srcResourceDir)
file(TO_NATIVE_PATH "${PROJECT_BINARY_DIR}/res" dstResourceDir)

if (WIN32) # Why doesn't TO_NATIVE_PATH work??? We have to manually replace with backslash for some annoying reason
    string(REPLACE "/" "\\" srcResourceDir ${srcResourceDir})
    string(REPLACE "/" "\\" dstResourceDir ${dstResourceDir})
endif()

if (${CREATE_RESOURCE_SYMLINK})
    message(STATUS "Build will create a symbolic link to resources folder: ${srcResourceDir}")
    if (UNIX)
        add_custom_target(copy_resources ALL
                COMMAND ${CMAKE_COMMAND} -E create_symlink "${srcResourceDir}" "${dstResourceDir}"
                COMMENT Creating resources symlink)
    elseif(WIN32)
        add_custom_target(copy_resources ALL
                COMMAND cmd.exe /c if exist \"${dstResourceDir}\" rmdir \"${dstResourceDir}\"
                COMMAND cmd.exe /c mklink /J \"${dstResourceDir}\" \"${srcResourceDir}\"
                COMMENT Creating resources symlink)
    else()
        message(FATAL_ERROR "Unable to create symbolic link to resources directory on this platform")
    endif()
else()
    message(STATUS "Build will copy resources folder: ${srcResourceDir}")
    add_custom_target(copy_resources ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${srcResourceDir}" "${dstResourceDir}"
            COMMENT Copying resources)
endif()

add_dependencies(${PROJECT_NAME} copy_resources)