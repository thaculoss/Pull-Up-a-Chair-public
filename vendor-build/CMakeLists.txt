cmake_minimum_required(VERSION 3.25)

# Relocatable counterpart of the recorded build, using only supplied source.
# No dependency is fetched and no dependency source file is changed here.
set(PUAC_VENDOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../vendor" CACHE PATH "Pinned dependency source snapshots")
get_filename_component(PUAC_VENDOR_ROOT "${PUAC_VENDOR_ROOT}" ABSOLUTE)
set(PUAC_V "${PUAC_VENDOR_ROOT}")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${PUAC_V}/fmt" "${CMAKE_CURRENT_BINARY_DIR}/fmt" EXCLUDE_FROM_ALL)
add_subdirectory("${PUAC_V}/spdlog" "${CMAKE_CURRENT_BINARY_DIR}/spdlog" EXCLUDE_FROM_ALL)

add_library(dxmath STATIC "${PUAC_V}/DirectXTK/Src/SimpleMath.cpp")
target_include_directories(dxmath PUBLIC "${PUAC_V}/DirectXTK/Inc" "${PUAC_V}/DirectXMath/Inc")
add_library(Microsoft::DirectXTK ALIAS dxmath)
add_library(puac_directxmath INTERFACE)
target_include_directories(puac_directxmath INTERFACE "${PUAC_V}/DirectXMath/Inc")
add_library(Microsoft::DirectXMath ALIAS puac_directxmath)

# OpenVR's original CMake writes into its source tree and uses CMAKE_SOURCE_DIR.
# Build the same API files here to produce the import library out of tree.
set(PUAC_OPENVR_SOURCES openvr_api_public.cpp jsoncpp.cpp
    vrcommon/dirtools_public.cpp vrcommon/envvartools_public.cpp
    vrcommon/pathtools_public.cpp vrcommon/sharedlibtools_public.cpp
    vrcommon/hmderrors_public.cpp vrcommon/vrpathregistry_public.cpp
    vrcommon/strtools_public.cpp)
list(TRANSFORM PUAC_OPENVR_SOURCES PREPEND "${PUAC_V}/openvr/src/")
add_library(puac_openvr SHARED ${PUAC_OPENVR_SOURCES})
set_target_properties(puac_openvr PROPERTIES OUTPUT_NAME openvr_api CXX_STANDARD 17)
target_include_directories(puac_openvr PUBLIC "${PUAC_V}/openvr/headers"
    PRIVATE "${PUAC_V}/openvr/src" "${PUAC_V}/openvr/src/vrcommon")
target_compile_definitions(puac_openvr PRIVATE WIN64 VR_API_PUBLIC _CRT_SECURE_NO_WARNINGS)
target_link_libraries(puac_openvr PRIVATE shell32 advapi32)

# Exact v11 source list, with all layout-critical runtime switches enabled.
# Optional feature switches reproduce the published v11 prebuilt's full superset
# internally; the original plugin only exposed the runtime/layout switches.
include("${PUAC_V}/CommonLibSSE-NG/cmake/sourcelist.cmake")
list(TRANSFORM SOURCES PREPEND "${PUAC_V}/CommonLibSSE-NG/")
add_library(CommonLibSSE STATIC ${SOURCES} "${PUAC_V}/MinHook/src/hde/hde64.c")
add_library(CommonLibSSE::CommonLibSSE ALIAS CommonLibSSE)
target_compile_features(CommonLibSSE PUBLIC cxx_std_23)
target_include_directories(CommonLibSSE PUBLIC "${PUAC_V}/CommonLibSSE-NG/include"
    "${PUAC_V}/openvr/headers" "${PUAC_V}/rapidcsv/src"
    "${PUAC_V}/DirectXTK/Inc" "${PUAC_V}/DirectXMath/Inc"
    PRIVATE "${PUAC_V}/MinHook/src/hde" "${PUAC_V}/simpleini"
    "${PUAC_V}/nlohmann-json/include" "${PUAC_V}/toml11/include" "${PUAC_V}/xbyak")
target_compile_definitions(CommonLibSSE PUBLIC ENABLE_SKYRIM_SE=1 ENABLE_SKYRIM_AE=1
    ENABLE_SKYRIM_VR=1 HAS_SKYRIM_MULTI_TARGETING=1 SPDLOG_COMPILED_LIB SPDLOG_FMT_EXTERNAL
    WINVER=0x0601 _WIN32_WINNT=0x0601
    PRIVATE COMMONLIB_VERSION="11.0.0" REX_OPTION_INI=1 REX_OPTION_JSON=1
    REX_OPTION_TOML=1 SKSE_SUPPORT_XBYAK=1 SKSE_SUPPORT_PATCH_SAFETY=1)
target_precompile_headers(CommonLibSSE PRIVATE "${PUAC_V}/CommonLibSSE-NG/include/SKSE/Impl/PCH.h")
set_source_files_properties("${PUAC_V}/MinHook/src/hde/hde64.c" PROPERTIES
    LANGUAGE C SKIP_PRECOMPILE_HEADERS ON COMPILE_OPTIONS "/WX-")
target_compile_options(CommonLibSSE PRIVATE /Zc:preprocessor /utf-8 /bigobj
    /wd4005 /wd4061 /wd4068 /wd4200 /wd4201 /wd4265 /wd4266 /wd4371
    /wd4514 /wd4582 /wd4583 /wd4623 /wd4625 /wd4626 /wd4702 /wd4710
    /wd4711 /wd4820 /wd4996 /wd5026 /wd5027 /wd5045 /wd5053 /wd5204 /wd5220)
target_link_libraries(CommonLibSSE PUBLIC spdlog::spdlog fmt::fmt dxmath puac_openvr
    advapi32 bcrypt d3d11 d3dcompiler dbghelp dxgi ole32 version windowscodecs)
include(CheckIPOSupported)
check_ipo_supported(RESULT PUAC_IPO_SUPPORTED)
if(PUAC_IPO_SUPPORTED)
    set_property(TARGET CommonLibSSE PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
include("${PUAC_V}/CommonLibSSE-NG/cmake/CommonLibSSE.cmake")

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_DX12 OFF CACHE BOOL "" FORCE)
set(BC_USE_OPENMP OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory("${PUAC_V}/nifly" "${CMAKE_CURRENT_BINARY_DIR}/nifly" EXCLUDE_FROM_ALL)
add_subdirectory("${PUAC_V}/DirectXTex" "${CMAKE_CURRENT_BINARY_DIR}/DirectXTex" EXCLUDE_FROM_ALL)
set(CMAKE_CXX_STANDARD 23)
set(PUAC_MRF "${PUAC_V}/MeshRenderingFramework")
add_library(food_renderer STATIC "${PUAC_MRF}/src/Mesh.cpp" "${PUAC_MRF}/src/RenderManager.cpp"
    "${PUAC_MRF}/src/GameAnimation.cpp" "${PUAC_MRF}/src/API.cpp")
target_include_directories(food_renderer PUBLIC "${PUAC_MRF}/include")
target_precompile_headers(food_renderer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/renderer-PCH.h")
target_link_libraries(food_renderer PUBLIC CommonLibSSE nifly DirectXTex windowscodecs)
target_compile_definitions(food_renderer PUBLIC UNICODE _UNICODE NOMINMAX WINVER=0x0601 _WIN32_WINNT=0x0601)
target_compile_options(food_renderer PRIVATE /utf-8 /Zc:preprocessor /permissive- /W3)
