cmake_minimum_required(VERSION 3.21)

project(3radials VERSION 1.0.0 LANGUAGES CXX)

# ============================================================
# CommonLibSSE-NG
# ============================================================

add_subdirectory(lib/commonlibsse-ng)

include(lib/commonlibsse-ng/cmake/CommonLibSSE.cmake)


# ============================================================
# Output
# ============================================================

if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
endif()


# ============================================================
# ImGui
# ============================================================

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp

    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

# ============================================================
# SKSE Plugin
# ============================================================

add_commonlibsse_plugin(${PROJECT_NAME}
    SOURCES
        src/plugin.cpp
        src/Menu/Menu.cpp
        src/Config/Config.cpp
        src/Config/Language.cpp
        src/Utils/Logger.cpp
        src/Config/Input.cpp
        src/Config/Gamepad.cpp
        src/Config/Font.cpp
        src/Animation/Animation.cpp
        src/Animation/RadialShape.cpp
        src/Animation/OverflowMechanism.cpp
        src/Animation/Track/TrackGeometry.cpp
        src/Animation/Track/TrackLayout.cpp
        src/Animation/Track/TrackEditor.cpp
        src/Animation/Track/TrackMovement.cpp
        src/Render/Icon.cpp
        src/Render/IconCustom.cpp
        src/Render/SvgRasterizer.cpp
        src/Item/Item.cpp
        src/Item/ItemInfo.cpp
        #src/Blur.cpp
        #src/InventoryHack.h
        src/Render/Preview.cpp 
        src/Utils/Serialization.cpp   
        #src/EffectControl.cpp
        #src/EffectDatabase.cpp
        src/Render/RenderHooks.cpp
        src/Render/RenderManager.cpp
        src/Render/Blur.cpp
        src/Render/Present.cpp
        src/Utils/Resolution.cpp
        src/Utils/Slowtime.cpp
        #src/PresentHook.cpp

        ${IMGUI_SOURCES}
)


# ============================================================
# C++ Standard
# ============================================================

target_compile_features(${PROJECT_NAME} PRIVATE
    cxx_std_23
)


# ============================================================
# Precompiled Header
# ============================================================

target_precompile_headers(${PROJECT_NAME} PRIVATE
    src/PCH.h
)


# ============================================================
# Include Directories
# ============================================================

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Render
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Item
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external/nanosvg/src
)


# ============================================================
# CommonLibSSE-NG
# ============================================================

target_link_libraries(${PROJECT_NAME} PRIVATE
    CommonLibSSE::CommonLibSSE

    d3d11
    dxgi
    d3dcompiler
    ole32
)


# ============================================================
# Definitions
# ============================================================

target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_DEFINE_MATH_OPERATORS
)


# ============================================================
# Deploy
# ============================================================

if(DEFINED OUTPUT_FOLDER)

    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD

        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${DLL_FOLDER}"

        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"

        VERBATIM
    )

    # Copy PDB on Debug
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")

        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD

            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
                "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"

            VERBATIM
        )

    endif()

endif()
