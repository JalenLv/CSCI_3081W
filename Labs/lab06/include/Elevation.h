#ifndef MAPVIEWER_ELEVATION_H_
#define MAPVIEWER_ELEVATION_H_

// Terrain heights from an OpenTopography Arc ASCII grid (AAIGrid), stored in
// apps/MapViewer/data/terrain (see README.md, 5.1 and 6.2). No GL here, so
// this can be used on its own.

#include <cstdint>
#include <string>
#include <vector>
#include "Geo.h"

class ElevationGrid {
public:
    // Loads the terrain for `scene` (lat/lon box of the frozen 3D scene) from
    // the best file in config::kTerrainDir, or from fetchDem() if no file
    // overlaps the scene. Returns false when there's no usable terrain.
    // `note` gets a message for the HUD; it can be set on success too, when
    // the file covers only part of the scene.
    bool load(const geo::LatLonBox& scene, std::string& note);

    // Parses Arc ASCII grid text and fills NODATA cells. Doesn't mark the
    // grid loaded; load() does that. Public for testing.
    bool parse(const std::string& text, std::string& error);

    void clear();

    bool loaded() const { return mLoaded; }
    float minHeight() const { return mMin; }   // lowest valid height in the scene
    float maxHeight() const { return mMax; }   // highest valid height in the scene
    const std::string& label() const { return mLabel; }  // e.g. "USGS10m, 10 m cells"
    const std::string& dataset() const { return mDataset; }  // e.g. "USGS10m"
    const geo::LatLonBox& bounds() const { return mBounds; }
    int columns() const { return mCols; }
    int rows() const { return mRows; }

    // Bilinear height in meters at a Web Mercator point. Outside the grid the
    // edge heights repeat.
    float sample(double mx, double my) const;
    float sampleLatLon(double lat, double lon) const;

private:
    float at(int c, int r) const { return mHeights[size_t(r) * mCols + c]; }
    void fillNoData();
    void computeRange(const geo::LatLonBox& scene);

    std::vector<float> mHeights;        // row 0 = north
    std::vector<uint8_t> mValid;        // 1 where the file had a real height
    int mCols = 0, mRows = 0;
    double mCellLon = 0.0, mCellLat = 0.0;  // cell size in degrees
    geo::LatLonBox mBounds;             // outer edges of the grid
    float mMin = 0.0f, mMax = 0.0f;
    std::string mLabel, mDataset;
    bool mLoaded = false;
};

#endif
