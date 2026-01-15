cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 75 86)
endif()

enable_language(C CXX CUDA)

find_package(CUDAToolkit REQUIRED)

file(GLOB_RECURSE Eulerian3D_SOURCE_FILES "./src/*.cpp")
file(GLOB_RECURSE Eulerian3D_HEADER_FILES "./include/*.h ./include/*.hpp")
file(GLOB_RECURSE Eulerian3D_CUDA_FILES "./cuda/*.cu")

source_group("Header Files" FILES ${Eulerian3D_HEADER_FILES})

add_library(eulerian3d STATIC "${Eulerian3D_SOURCE_FILES}" "${Eulerian3D_HEADER_FILES}" "${Eulerian3D_CUDA_FILES}")
target_include_directories(eulerian3d PRIVATE "./include")

set_target_properties(eulerian3d PROPERTIES 
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON
    CUDA_ARCHITECTURES "75;86"
)

include_directories("./include")

if(WIN32)
    target_link_libraries(eulerian3d PRIVATE CUDA::cudart opengl32)
else()
    target_link_libraries(eulerian3d PRIVATE CUDA::cudart)
endif()

# common
target_link_libraries(eulerian3d PRIVATE common)

# glfw
target_link_libraries(eulerian3d PRIVATE "${PROJECT_SOURCE_DIR}/third_party/glfw/lib/glfw3.lib")