cmake_minimum_required(VERSION 3.21)
project(WorldEngine)

set(CMAKE_CXX_STANDARD 20)

set(SDL2_PATH "D:/Code/Libraries/SDL2-2.0.20")
set(VULKAN_PATH "D:/Code/VulkanSDK/1.2.198.1")
set(GLM_PATH "D:/Code/Libraries/glm-0.9.9.8")
set(ENTT_PATH "D:/Code/Libraries/entt")
set(STB_IMAGE_PATH "D:/Code/Libraries/stb")

#IF(CMAKE_COMPILER_IS_GNUCC)
#    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O1")
#    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O1")
#ENDIF(CMAKE_COMPILER_IS_GNUCC)

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




include_directories(
        ${PROJECT_SOURCE_DIR}/src
        ${SDL2_INCLUDE_DIR}
        ${Vulkan_INCLUDE_DIR}
        ${GLM_INCLUDE_DIR}
        ${ENTT_INCLUDE_DIR}
        ${STB_IMAGE_INCLUDE_DIR})

link_directories(
        ${SDL2_LIB_DIR})

file(GLOB_RECURSE PROJECT_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.c
        ${CMAKE_SOURCE_DIR}/src/*.hpp
        ${CMAKE_SOURCE_DIR}/src/*.h)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_FILES})

target_link_libraries(${PROJECT_NAME}
        ${Vulkan_LIBRARIES}
        ${SDL2_LIBRARIES}
        ${STB_IMAGE_LIBRARIES})


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