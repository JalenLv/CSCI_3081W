#ifndef MAPVIEWER_MAPVIEWER_H_
#define MAPVIEWER_MAPVIEWER_H_

// The MapViewer app: ArcGIS World Imagery in 2D, and a 3D view with terrain,
// buildings and routes. The window class itself lives in MapViewer.cpp, so
// this header pulls in no GL or NanoGUI headers.

class MapViewer {
public:
    // Sets up curl and the cache folders, opens the window and runs until it
    // closes. Returns the process exit code. Run from the repository root so
    // the data paths resolve.
    int run();
};

#endif
