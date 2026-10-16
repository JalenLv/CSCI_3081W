cmake_minimum_required(VERSION 3.5)
project(MapViewer)

set(CMAKE_CXX_STANDARD 11)
set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_BUILD_TYPE Debug)

find_package(OpenGL REQUIRED)

set(GOPHERMAP_DIR ${CMAKE_SOURCE_DIR}/apps/GopherMap)
set(NANOGUI_DIR   ${CMAKE_SOURCE_DIR}/lib/NanoGUI)

# No pkg-config glfw3 here: NanoGUI bundles its own GLFW (compiled into
# nanogui.a), so we use its headers and its library. Linking a second GLFW
# alongside it is asking for trouble.
include_directories(
     ${OPENGL_INCLUDE_DIR}
     ${CMAKE_SOURCE_DIR}/lib                 # glad/glad.h (must win over NanoGUI's copy)
     ${CMAKE_SOURCE_DIR}/lib/glm
     ${CMAKE_SOURCE_DIR}/include
     ${CMAKE_SOURCE_DIR}/include/graphics
     ${GOPHERMAP_DIR}                        # WebService.h, json.hpp
     ${NANOGUI_DIR}/install/include
     ${NANOGUI_DIR}/src/ext/eigen
     ${NANOGUI_DIR}/src/ext/nanovg/src
     ${NANOGUI_DIR}/src/ext/glfw/include
)

link_directories(${NANOGUI_DIR}/install/lib)

add_definitions(-DGLM_FORCE_INLINE)

add_executable(MapViewer
     main.cpp          # creates and runs a MapViewer
     MapViewer.cpp     # window: GUI, input, 2D map, 3D view, HUD
     Graphics.cpp      # shaders, meshes, imagery tiles (the only stb_image copy)
     Elevation.cpp     # terrain grid (Arc ASCII) from data/terrain
     Buildings.cpp     # OSM buildings from data/osm, extruded
     Routes.cpp        # route graph from OSM roads in data/osm
     Fetchers.cpp      # PLACEHOLDER downloads for terrain and buildings
     ${GOPHERMAP_DIR}/WebService.cpp
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
     find_package(Threads REQUIRED)
     find_package(X11 REQUIRED)
     set(DEP_LIBS ${CMAKE_THREAD_LIBS_INIT} rt Xrandr Xxf86vm Xinerama Xcursor Xi m dl ${X11_LIBRARIES})
endif()

if (APPLE)
     set(DEP_LIBS
          "-framework Cocoa"
          "-framework IOKit"
          "-framework CoreFoundation"
          "-framework CoreVideo")
endif()

# target_link_libraries(MapViewer ggis_graphics nanogui.a OpenGL::GL curl ${DEP_LIBS})
target_link_libraries(MapViewer ggis_graphics
     ${NANOGUI_DIR}/install/lib/libnanogui.a
     OpenGL::GL curl ${DEP_LIBS})
add_dependencies(MapViewer NanoGUI)
