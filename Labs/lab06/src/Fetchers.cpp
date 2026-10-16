// PLACEHOLDER fetchers. See Fetchers.h for the contract and README.md
// (5.1 to 5.4 and 7) for the exact requests these will make.
//
// Until they're written, the viewer only uses files that are already in
// apps/MapViewer/data, and the HUD shows which box to download by hand.

#include "Fetchers.h"

std::string fetchDem(const geo::LatLonBox& box, std::string& error) {
    // TODO(fetcher): GET
    //   https://portal.opentopography.org/API/usgsdem?datasetName=<config::kDemDataset>
    //     &south=..&north=..&west=..&east=..&outputFormat=AAIGrid&API_Key=<config::kOpenTopoApiKey>
    //   (globaldem with demtype=... for datasets outside the US).
    //   Map HTTP 204 -> "no data", 401 -> "bad API key", others -> "HTTP <code>".
    (void)box;
    error = "terrain fetcher not implemented yet";
    return "";
}

std::string fetchOsmBuildings(const geo::LatLonBox& box, std::string& error) {
    // TODO(fetcher): POST to https://overpass-api.de/api/interpreter with
    //   data=[out:json][timeout:90];
    //        (way["building"]["building"!="no"](S,W,N,E);
    //         relation["building"]["type"="multipolygon"](S,W,N,E););
    //        out geom;
    //   Map HTTP 429 / 504 -> "server busy". A 200 response can still carry a
    //   "remark" (e.g. timeout); the viewer reports that itself.
    (void)box;
    error = "buildings fetcher not implemented yet";
    return "";
}

std::string fetchOsmRoads(const geo::LatLonBox& box, std::string& error) {
    // TODO(fetcher): same as fetchOsmBuildings, with the roads query from
    //   README.md, 5.3:
    //   [out:json][timeout:120];
    //   (way["highway"~"^(motorway|trunk|primary|secondary|tertiary|unclassified|residential|
    //        living_street|motorway_link|trunk_link|primary_link|secondary_link|tertiary_link)$"]
    //        ["area"!="yes"](S,W,N,E);
    //    way["highway"="service"]["service"!~"^(parking_aisle|driveway|drive-through)$"]
    //        ["area"!="yes"](S,W,N,E););
    //   out geom;
    (void)box;
    error = "roads fetcher not implemented yet";
    return "";
}
