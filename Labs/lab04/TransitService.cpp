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
    json result = ws.GetJSON("/nextrip/routes");
    for (unsigned int i = 0; i < result.size(); i++) {
        Route route {
            result[i]["route_id"].get<std::string>(),
            result[i]["route_label"].get<std::string>(),
            result[i]["agency_id"].get<int>()
        };
        routes.push_back(route);
    }

    return routes;
}


// **************************** Milestone 3 ****************************
// Implement GetDirecitons(...), GetStops(...), and GetStopDetail(...)
std::vector<Direction> TransitService::GetDirections(const std::string& routeId) {
    std::vector<Direction> directions;

    json result = ws.GetJSON("/nextrip/directions/" + routeId);
    for (unsigned int i = 0; i < result.size(); i++) {
        Direction direction {
            result[i]["direction_id"].get<int>(),
            result[i]["direction_name"].get<std::string>()
        };
        directions.push_back(direction);
    }

    return directions;
}

std::vector<Stop> TransitService::GetStops(const std::string& routeId, int direction) {
    std::vector<Stop> stops;

    json result = ws.GetJSON("/nextrip/stops/" + routeId + "/" + std::to_string(direction));
    for (unsigned int i = 0; i < result.size(); i++) {
        Stop stop {
            result[i]["place_code"].get<std::string>(),
            result[i]["description"].get<std::string>()
        };
        stops.push_back(stop);
    }

    return stops;
}

std::vector<StopDetail> TransitService::GetStopDetail(const std::string& routeId, int direction, const std::string& placeCode) {
    // struct StopDetail {
    //     float longitude;
    //     float latitude;
    //     std::string nextDepartureText;
    // };
    std::vector<StopDetail> stopDetails;

    json result = ws.GetJSON("/nextrip/" + routeId + "/" + std::to_string(direction) + "/" + placeCode);
    float latitude  = result["stops"][0]["latitude"].get<float>();
    float longitude = result["stops"][0]["longitude"].get<float>();
    for (unsigned int i = 0; i < result["departures"].size(); i++) {
        StopDetail stopDetail {
            longitude, latitude,
            result["departures"][i]["departure_text"].get<std::string>()
        };
        stopDetails.push_back(stopDetail);
    }

    return stopDetails;
}

}