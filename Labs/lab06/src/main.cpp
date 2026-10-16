/*
    MapViewer -- ArcGIS World Imagery viewer with a NanoGUI control panel.

    2D map mode
      Camera: a center point (Web Mercator, double precision) plus a
      fractional zoom. Dragging, scrolling, Zoom in/out and Go only move the
      camera. Nothing is fetched until Render is pressed; Render downloads
      the tiles covering the current view at the nearest whole zoom level and
      discards everything else.

    3D view
      "3D view" freezes the last render (rendering first if the view has
      changed) into a 3D scene in local meters. With Elevation on, each
      imagery tile becomes a grid mesh draped over terrain from an
      OpenTopography grid; buildings and the road graph come from
      OpenStreetMap (see README.md). Orbit / pan / zoom like a CAD or
      Blender viewport; checkboxes show and hide object types.
      "Back to 2D map" eases back to top-down and returns to map mode.

    Run from the repository root so the cache paths resolve.
*/

#include "MapViewer.h"

int main(int /* argc */, char** /* argv */) {
    MapViewer viewer;
    return viewer.run();
}
