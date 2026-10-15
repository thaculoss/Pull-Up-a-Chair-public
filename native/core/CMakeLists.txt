cmake_minimum_required(VERSION 3.25)
project(PullUpAChairOrdering VERSION 0.9.9 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
if(NOT PUAC_POLICY_TESTS_ONLY)
add_library(PullUpAChairOrdering SHARED src/main.cpp src/InnServices.cpp src/SupplierServices.cpp src/TableManager.cpp src/DailyMenus.cpp src/BarterPricing.cpp src/TableCleanup.cpp src/Specialties.cpp src/TableDiscovery.cpp src/CallService.cpp src/CallFocus.cpp src/CallSettingsUI.cpp src/ServiceApproach.cpp src/SkyPromptCall.cpp src/ServicePriority.cpp src/ServingSize.cpp src/TopicVisibility.cpp src/SettingsStore.cpp)
target_sources(PullUpAChairOrdering PRIVATE src/ServiceSceneReservation.cpp)
target_include_directories(PullUpAChairOrdering PRIVATE src include "${PUAC_VENDOR_ROOT}/MeshRenderingFramework/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SkyPromptAPI/include"
    "${PUAC_VENDOR_ROOT}/nifly/include" "${PUAC_VENDOR_ROOT}/nifly/external"
    "${PUAC_VENDOR_ROOT}/DirectXTex/DirectXTex")
target_link_libraries(PullUpAChairOrdering PRIVATE CommonLibSSE food_renderer
    nifly DirectXTex)
target_commonlibsse_properties(PullUpAChairOrdering NAME "PullUpAChairOrdering" AUTHOR "Pull Up a Chair" VERSION 0.9.9 USE_ADDRESS_LIBRARY)

add_library(PullUpAChairEAS SHARED adapter/src/main.cpp)
# Build the pinned, unchanged MinHook sources from this project tree.
add_library(PUACMinHook STATIC
    third_party/minhook/src/buffer.c
    third_party/minhook/src/hook.c
    third_party/minhook/src/trampoline.c
    third_party/minhook/src/hde/hde64.c)
target_include_directories(PUACMinHook PUBLIC third_party/minhook/include
    PRIVATE third_party/minhook/src third_party/minhook/src/hde)
target_include_directories(PullUpAChairEAS PRIVATE adapter/src src)
target_link_libraries(PullUpAChairEAS PRIVATE CommonLibSSE PUACMinHook)
target_commonlibsse_properties(PullUpAChairEAS NAME "PullUpAChairEAS" AUTHOR "Pull Up a Chair optional dining and EAS module" VERSION 0.9.9 USE_ADDRESS_LIBRARY)
foreach(plugin PullUpAChairOrdering PullUpAChairEAS)
    target_precompile_headers(${plugin} PRIVATE src/PCH.h)
    target_compile_definitions(${plugin} PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0601 _WIN32_WINNT=0x0601)
    target_compile_options(${plugin} PRIVATE /utf-8 /Zc:preprocessor /permissive- /W4 /Zi /bigobj)
    target_link_options(${plugin} PRIVATE /DEBUG /OPT:REF /OPT:ICF)
endforeach()

endif() # native plugins

enable_testing()
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/OrderSession.inl" CONFIRMATION_SOURCE)
string(FIND "${CONFIRMATION_SOURCE}" "void Snapshot(int token)" CONFIRMATION_END)
if(CONFIRMATION_END LESS 1)
    message(FATAL_ERROR "Cannot locate production order confirmation callback")
endif()
string(SUBSTRING "${CONFIRMATION_SOURCE}" 0 ${CONFIRMATION_END} CONFIRMATION_EXCERPT)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/confirmation-fixture")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/confirmation-fixture/OrderConfirmationExcerpt.inl" "${CONFIRMATION_EXCERPT}")
add_executable(confirmation_tests tests/confirmation_tests.cpp)
target_include_directories(confirmation_tests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/confirmation-fixture")
target_compile_options(confirmation_tests PRIVATE /utf-8 /W4)
add_test(NAME gold_preflight_and_queued_confirmation COMMAND confirmation_tests)
set(FONT_TEST_IMGUI "${PUAC_VENDOR_ROOT}/imgui-preview")
add_executable(menu_typeface_tests tests/menu_typeface_tests.cpp
    "${FONT_TEST_IMGUI}/imgui.cpp" "${FONT_TEST_IMGUI}/imgui_draw.cpp"
    "${FONT_TEST_IMGUI}/imgui_tables.cpp" "${FONT_TEST_IMGUI}/imgui_widgets.cpp")
target_include_directories(menu_typeface_tests PRIVATE src "${FONT_TEST_IMGUI}")
target_compile_options(menu_typeface_tests PRIVATE /utf-8 /W3)
add_test(NAME scoped_chinese_menu_typeface COMMAND menu_typeface_tests
    "${PROJECT_SOURCE_DIR}/../../test-data/fonts/PUAC-WenKai-Medium.ttf")
if(NOT PUAC_POLICY_TESTS_ONLY)
add_executable(menu_preview preview/main.cpp "${FONT_TEST_IMGUI}/imgui.cpp"
    "${FONT_TEST_IMGUI}/imgui_draw.cpp" "${FONT_TEST_IMGUI}/imgui_tables.cpp"
    "${FONT_TEST_IMGUI}/imgui_widgets.cpp" "${FONT_TEST_IMGUI}/backends/imgui_impl_dx11.cpp")
target_include_directories(menu_preview PRIVATE src "${FONT_TEST_IMGUI}"
    "${FONT_TEST_IMGUI}/backends"
    "${PUAC_VENDOR_ROOT}/fmt/include" "${PUAC_VENDOR_ROOT}/DirectXTex/DirectXTex")
target_compile_definitions(menu_preview PRIVATE UNICODE _UNICODE NOMINMAX "ImTextureID=void*")
target_compile_options(menu_preview PRIVATE /utf-8 /W3)
target_link_libraries(menu_preview PRIVATE DirectXTex
    fmt::fmt d3d11 d3dcompiler dxgi ole32 windowscodecs)

endif() # native renderer preview

add_executable(inn_routine_tests tests/inn_routine_tests.cpp)
target_include_directories(inn_routine_tests PRIVATE src)
target_compile_options(inn_routine_tests PRIVATE /utf-8 /W4)
add_test(NAME service_control_rollback_regressions COMMAND inn_routine_tests)
add_executable(scene_reservation_tests tests/scene_reservation_tests.cpp)
target_include_directories(scene_reservation_tests PRIVATE src)
target_compile_options(scene_reservation_tests PRIVATE /utf-8 /W4)
add_test(NAME temporary_inn_chatter_reservation COMMAND scene_reservation_tests)
add_executable(service_scene_tests tests/service_scene_tests.cpp)
target_include_directories(service_scene_tests PRIVATE src)
target_compile_options(service_scene_tests PRIVATE /utf-8 /W4)
add_test(NAME bounded_scene_stop_and_chatty_compatibility COMMAND service_scene_tests)
add_executable(core_tests tests/core_tests.cpp)
target_include_directories(core_tests PRIVATE src)
target_compile_options(core_tests PRIVATE /utf-8 /W4)
add_test(NAME core_orders_tables_languages_runtimes COMMAND core_tests "${CMAKE_CURRENT_SOURCE_DIR}/config")
add_executable(low_table_tests tests/low_table_tests.cpp)
target_include_directories(low_table_tests PRIVATE src)
target_compile_options(low_table_tests PRIVATE /utf-8 /W4)
add_test(NAME vanilla_low_tables_registration COMMAND low_table_tests "${CMAKE_CURRENT_SOURCE_DIR}/config"
    "${PROJECT_SOURCE_DIR}/../../test-data/lowtable-before")
add_executable(adapter_tests tests/adapter_tests.cpp)
target_include_directories(adapter_tests PRIVATE adapter/src)
add_test(NAME retained_animation_identity_regression COMMAND adapter_tests)
add_executable(food_pack_tests tests/food_pack_tests.cpp)
target_include_directories(food_pack_tests PRIVATE src)
target_compile_options(food_pack_tests PRIVATE /utf-8 /W4)
add_test(NAME audited_food_pack_loading COMMAND food_pack_tests "${CMAKE_CURRENT_SOURCE_DIR}/config")
add_executable(supplier_tests tests/supplier_tests.cpp)
target_include_directories(supplier_tests PRIVATE src)
target_compile_options(supplier_tests PRIVATE /utf-8 /W4)
add_test(NAME stable_inn_suppliers COMMAND supplier_tests)
add_executable(call_tests tests/call_tests.cpp)
target_include_directories(call_tests PRIVATE src)
target_compile_options(call_tests PRIVATE /utf-8 /W4)
add_test(NAME switchable_call_entry COMMAND call_tests)

add_executable(service_bridge_tests tests/service_bridge_tests.cpp)
target_include_directories(service_bridge_tests PRIVATE src include)
target_compile_options(service_bridge_tests PRIVATE /utf-8 /W4)
add_test(NAME unified_service_leases_and_abi COMMAND service_bridge_tests)
add_executable(serving_size_tests tests/serving_size_tests.cpp)
target_include_directories(serving_size_tests PRIVATE src)
target_compile_options(serving_size_tests PRIVATE /utf-8 /W4)
add_test(NAME optional_compact_main_servings COMMAND serving_size_tests "${CMAKE_CURRENT_SOURCE_DIR}/config")

add_executable(settings_store_tests tests/settings_store_tests.cpp src/SettingsStore.cpp)
target_include_directories(settings_store_tests PRIVATE src)
target_compile_definitions(settings_store_tests PRIVATE NOMINMAX)
target_compile_options(settings_store_tests PRIVATE /utf-8 /W4)
add_test(NAME verified_shared_settings_persistence COMMAND settings_store_tests)


# Core-owned dining receipts and optional module boundary.
add_executable(dining_tests tests/dining_tests.cpp)
target_include_directories(dining_tests PRIVATE src adapter/src)
target_compile_options(dining_tests PRIVATE /utf-8 /W4)
add_test(NAME dining_claims_and_optional_interactions COMMAND dining_tests)

# Exercise the actual production core receipt functions at a fake engine boundary.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/TableManager.cpp" DINING_SOURCE)
string(FIND "${DINING_SOURCE}" "bool QueryServing(" DINING_START)
string(FIND "${DINING_SOURCE}" "void Install()" DINING_END)
math(EXPR DINING_LENGTH "${DINING_END}-${DINING_START}")
string(SUBSTRING "${DINING_SOURCE}" ${DINING_START} ${DINING_LENGTH} DINING_EXCERPT)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dining-fixture")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/dining-fixture/DiningCoreExcerpt.inl" "${DINING_EXCERPT}")
add_executable(dining_core_tests tests/dining_core_tests.cpp)
target_include_directories(dining_core_tests PRIVATE src adapter/src "${CMAKE_CURRENT_BINARY_DIR}/dining-fixture")
target_compile_options(dining_core_tests PRIVATE /utf-8 /W4)
add_test(NAME production_dining_receipts_and_inventory_rollback COMMAND dining_core_tests)

# Use the actual removal and serialized-recovery functions, not a second model.
foreach(part Removal Save Load)
    if(part STREQUAL "Removal")
        set(START_MARK "bool Matches(")
        set(END_MARK "void Dispatch(")
    elseif(part STREQUAL "Save")
        set(START_MARK "void Save(")
        set(END_MARK "int NextOrder(")
    else()
        set(START_MARK "void Revert()")
        set(END_MARK "bool QueryServing(")
    endif()
    string(FIND "${DINING_SOURCE}" "${START_MARK}" PART_START)
    string(FIND "${DINING_SOURCE}" "${END_MARK}" PART_END)
    math(EXPR PART_LENGTH "${PART_END}-${PART_START}")
    string(SUBSTRING "${DINING_SOURCE}" ${PART_START} ${PART_LENGTH} PART_EXCERPT)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/dining-fixture/Retirement${part}Excerpt.inl" "${PART_EXCERPT}")
endforeach()
add_executable(retirement_tests tests/retirement_tests.cpp)
target_include_directories(retirement_tests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/dining-fixture")
target_compile_options(retirement_tests PRIVATE /utf-8 /W4)
add_test(NAME removed_servings_survive_save_load COMMAND retirement_tests)
