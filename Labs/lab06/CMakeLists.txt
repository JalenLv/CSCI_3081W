cmake_minimum_required(VERSION 3.10)
project(MapViewer)

set(CMAKE_CXX_STANDARD 11) # no explicit compiler flags if possible
set(CMAKE_BUILD_TYPE Debug)
set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenGL REQUIRED)

include(ExternalProject)
ExternalProject_add(glm
     GIT_REPOSITORY https://github.com/g-truc/glm
     GIT_TAG 1.0
     SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/glm
     UPDATE_COMMAND ""
     BUILD_COMMAND ""
     CONFIGURE_COMMAND ""
     INSTALL_COMMAND ""
)

set(NanoGUIArgs
     -DCMAKE_POLICY_VERSION_MINIMUM=3.5      
     -DNANOGUI_BUILD_SHARED=OFF
     -DNANOGUI_BUILD_PYTHON=OFF
     -DNANOGUI_BUILD_EXAMPLE=OFF
     -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/lib/NanoGUI/install
     -DCMAKE_C_FLAGS=-fPIC
     -DCMAKE_CXX_FLAGS=-fPIC
     -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
     -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
     -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
     -DCMAKE_AR=${CMAKE_AR}
     -DCMAKE_DEBUG_POSTFIX=d
)
ExternalProject_add(NanoGUI
     GIT_REPOSITORY https://github.com/wjakob/nanogui.git
     CMAKE_ARGS ${NanoGUIArgs}
     SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/NanoGUI/src
     BINARY_DIR ${CMAKE_SOURCE_DIR}/lib/NanoGUI/build
     INSTALL_DIR ${CMAKE_SOURCE_DIR}/lib/NanoGUI/install
     UPDATE_COMMAND ""
     PATCH_COMMAND sed -i.bak "1s/.*/cmake_minimum_required(VERSION 3.5)/" resources/bin2c.cmake
     BUILD_BYPRODUCTS ${CMAKE_SOURCE_DIR}/lib/NanoGUI/install/lib/libnanogui.a
)

set(NANOGUI_DIR ${CMAKE_SOURCE_DIR}/lib/NanoGUI)

add_definitions(-DGLM_FORCE_INLINE)

# No pkg-config glfw3 here: NanoGUI bundles its own GLFW (compiled into
# nanogui.a), so we use its headers and its library. Linking a second GLFW
# alongside it is asking for trouble.
include_directories(
     ${OPENGL_INCLUDE_DIR}
     ${CMAKE_SOURCE_DIR}/lib                 # glad/glad.h (must win over NanoGUI's copy)
     ${CMAKE_SOURCE_DIR}/lib/glm
     ${CMAKE_SOURCE_DIR}/include
     ${NANOGUI_DIR}/install/include
     ${NANOGUI_DIR}/src/ext/eigen
     ${NANOGUI_DIR}/src/ext/glad/include
     ${NANOGUI_DIR}/src/ext/nanovg/src
     ${NANOGUI_DIR}/src/ext/glfw/include
)

link_directories(${NANOGUI_DIR}/install/lib)

# glad.c comes from the NanoGUI clone, which only appears at build time, so this tell CMake not to look for it during configure. (Without this it fails on MacOS)
set(NANOGUI_GLAD_C ${NANOGUI_DIR}/src/ext/glad/src/glad.c)
set_source_files_properties(${NANOGUI_GLAD_C} PROPERTIES GENERATED TRUE)

add_executable(MapViewer
     src/main.cpp          # creates and runs a MapViewer
     src/MapViewer.cpp     # window: GUI, input, 2D map, 3D view, HUD
     src/Graphics.cpp      # shaders, meshes, imagery tiles (the only stb_image copy)
     src/Elevation.cpp     # terrain grid (Arc ASCII) from data/terrain
     src/Buildings.cpp     # OSM buildings from data/osm, extruded
     src/Routes.cpp        # route graph from OSM roads in data/osm
     src/Fetchers.cpp      # PLACEHOLDER downloads for terrain and buildings
     src/WebService.cpp
     ${NANOGUI_GLAD_C}
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
     find_package(Threads REQUIRED)
     find_package(X11 REQUIRED)
     set(DEP_LIBS ${CMAKE_THREAD_LIBS_INIT} rt Xrandr Xxf86vm Xinerama Xcursor Xi m dl ${X11_LIBRARIES})
endif()

if (APPLE)
     add_definitions(-DGL_SILENCE_DEPRECATION)   # Apple marks all OpenGL calls deprecated
     set(DEP_LIBS
          "-framework Cocoa"
          "-framework IOKit"
          "-framework CoreFoundation"
          "-framework CoreVideo")
endif()

target_link_libraries(MapViewer
     ${NANOGUI_DIR}/install/lib/libnanogui.a
     OpenGL::GL curl ${DEP_LIBS})
add_dependencies(MapViewer NanoGUI glm)
