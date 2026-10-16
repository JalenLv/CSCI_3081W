#ifndef MAPVIEWER_FETCHERS_H_
#define MAPVIEWER_FETCHERS_H_

// Downloads for data that isn't on disk yet. The viewer calls these only
// when no file in apps/MapViewer/data covers the scene (README.md,
// 6.1).
//
// Contract (README.md, 7):
//   - Return the raw response body exactly as the server sent it, or "" and
//     set `error` to a short message for the HUD.
//   - Don't parse the body and don't write files. The viewer validates the
//     body and saves it under the right name, like tiles::fetchAll does.
//   - Called on the main thread; no GL work.
//
// PLACEHOLDERS: all three return "" for now. The real versions will replace
// Fetchers.cpp; keep these signatures so the rest of the app doesn't
// change.

#include <string>
#include "Geo.h"

// OpenTopography Arc ASCII grid (outputFormat=AAIGrid) for `box`, dataset
// config::kDemDataset.
std::string fetchDem(const geo::LatLonBox& box, std::string& error);

// Overpass API JSON (`out geom`) with the building ways and multipolygon
// relations in `box`.
std::string fetchOsmBuildings(const geo::LatLonBox& box, std::string& error);

// Overpass API JSON (`out geom`) with the drivable road ways in `box`.
std::string fetchOsmRoads(const geo::LatLonBox& box, std::string& error);

#endif
