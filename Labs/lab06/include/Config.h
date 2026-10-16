#ifndef MAPVIEWER_CONFIG_H_
#define MAPVIEWER_CONFIG_H_

// All tunable settings in one place. Paths are relative to the repository
// root, which is where the app must be run from.

#include <string>

namespace config {
const char* const kImageryServer =
    "https://server.arcgisonline.com/arcgis/rest/services/World_Imagery/MapServer/tile/";

const std::string kCacheDir = "data";
const std::string kImageryCacheDir = kCacheDir + "/imagery";
const std::string kTerrainDir = kCacheDir + "/terrain";   // dem_<dataset>_<S>_<W>_<N>_<E>.asc
const std::string kOsmDir = kCacheDir + "/osm";  // buildings_<S>_<W>_<N>_<E>.json, roads_<S>_<W>_<N>_<E>.json

// OpenGL version of the window. macOS supports OpenGL up to 4.1 (core profile).
const int kGlMajor = 4;
#if defined(__APPLE__)
const int kGlMinor = 1;
#else
const int kGlMinor = 3;
#endif

const double kStartLat = 44.974;     // UMN campus
const double kStartLon = -93.228;
const double kStartZoom = 15.0;

const double kMinZoom = 2.0;
const double kMaxZoom = 19.0;
const int kMinFeatureZoom = 15;      // routes / buildings need at least this
const double kScrollZoomStep = 0.25; // 2D zoom change per scroll-wheel notch

const int kTileSize = 256;
const int kDownloadThreads = 8;
const int kMaxTilesPerRender = 300;

// Terrain (see README.md)
const char* const kDemDataset = "USGS10m";  // dataset the DEM fetcher asks for
const char* const kOpenTopoApiKey = "";     // for the fetcher; don't commit a real key
const int kTerrainGridPerTile = 32;         // mesh cells per imagery tile edge
const float kMaxExaggeration = 10.0f;

// When no data file covers the scene, the fetchers are asked for the scene
// box grown by this fraction on each side and rounded outward to 0.001 deg.
const double kFetchPadding = 0.1;

// Buildings (see README.md, 6.3)
const float kMetersPerLevel = 3.0f;
const float kDefaultBuildingHeight = 8.0f;

// 3D view
const float kFovY = 45.0f;           // degrees
const float kMaxPitch = 85.0f;       // degrees from straight down
const float kOrbitDegPerPixel = 0.3f;
const float kScrollDollyFactor = 0.85f;
const double kAnimSeconds = 0.45;
}

#endif
