#include "TransitService.h"
#include <iostream>

namespace MetroTransitAPI {

//------------------- MetroTransitAPI -------------------------

std::vector<Agency> TransitService::GetAgencies() {
    std::vector<Agency> agencies;
    json result = ws.GetJSON("/nextrip/agencies");

    // Debug Code:
    // std::cout << result << std::endl;
    
    for (unsigned int i = 0; i < result.size(); i++) {
        Agency agency;
        agency.id = result[i]["agency_id"].get<int>();
        agency.name = result[i]["agency_name"].get<std::string>();
        agencies.push_back(agency);   
    }
    return agencies;
}

// **************************** Milestone 2 ****************************
// Implement the GetRoutes() function
std::vector<Route> TransitService::GetRoutes() {
    std::vector<Route> routes;
    
    // TODO: Get routes

    return routes;
}


// **************************** Milestone 3 ****************************
// Implement GetDirecitons(...), GetStops(...), and GetStopDetail(...)
std::vector<Direction> TransitService::GetDirections(const std::string& routeId) {
    return std::vector<Direction>();
}

std::vector<Stop> TransitService::GetStops(const std::string& routeId, int direction) {
    return std::vector<Stop>();
}

std::vector<StopDetail> TransitService::GetStopDetail(const std::string& routeId, int direction, const std::string& placeCode) {
    return std::vector<StopDetail>();
}

}