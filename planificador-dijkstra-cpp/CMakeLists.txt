cmake_minimum_required(VERSION 3.16)

project(PlanificadorDijkstraBogota
    VERSION 1.0.0
    DESCRIPTION "Planificador de Rutas con Dijkstra (Bogotá) en C++17 con raylib y Dear ImGui"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ==============================================================================
# 1. Dependencia: nlohmann/json (Serialización JSON)
# ==============================================================================
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# ==============================================================================
# 2. Dependencia: raylib v5.5 (Gráficos 2D y Ventana)
# ==============================================================================
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# ==============================================================================
# 3. Dependencias: Dear ImGui (v1.92.7) + rlImGui (Raylib_5_5)
# ==============================================================================
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.7
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG Raylib_5_5
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(rlimgui)
if(NOT rlimgui_POPULATED)
    FetchContent_Populate(rlimgui)
endif()

add_library(rlimgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(rlimgui_lib SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${rlimgui_SOURCE_DIR}
)
target_compile_definitions(rlimgui_lib PUBLIC NO_FONT_AWESOME)
target_link_libraries(rlimgui_lib PUBLIC raylib)

# ==============================================================================
# 4. Biblioteca Núcleo del Proyecto (Estructuras de Datos y Algoritmo de Dijkstra)
# ==============================================================================
add_library(dijkstra_core STATIC
    src/Haversine.cpp
    src/MinHeap.cpp
    src/Grafo.cpp
    src/Dijkstra.cpp
)
target_include_directories(dijkstra_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(dijkstra_core PUBLIC
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(dijkstra_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(dijkstra_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ==============================================================================
# 5. Ejecutable Principal (Aplicación Interactiva de Escritorio)
# ==============================================================================
add_executable(planificador_bogota
    src/main.cpp
)
target_link_libraries(planificador_bogota PRIVATE
    dijkstra_core
    rlimgui_lib
    raylib
)
if(MSVC)
    target_compile_options(planificador_bogota PRIVATE /W4 /utf-8)
else()
    target_compile_options(planificador_bogota PRIVATE -Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
endif()

# Copiar carpetas assets/ y data/ al directorio de compilación
add_custom_command(TARGET planificador_bogota POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:planificador_bogota>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:planificador_bogota>/data
)

# ==============================================================================
# 6. Pruebas Unitarias con Catch2 (v3)
# ==============================================================================
include(CTest)
if(BUILD_TESTING)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(test_dijkstra
        tests/test_dijkstra.cpp
    )
    target_link_libraries(test_dijkstra PRIVATE
        dijkstra_core
        Catch2::Catch2WithMain
    )

    add_custom_command(TARGET test_dijkstra POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/data
            $<TARGET_FILE_DIR:test_dijkstra>/data
    )

    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(test_dijkstra)
endif()
