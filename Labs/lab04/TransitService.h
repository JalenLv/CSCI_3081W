#ifndef TRANSIT_SERVICE_H_
#define TRANSIT_SERVICE_H_

#include <vector>
#include <string>

#include "WebService.h"

namespace MetroTransitAPI {

struct Route {
    std::string id;
    std::string name;
    int agencyId;
};

struct Agency {
    int id;
    std::string name;
};

struct Direction {
    int id;
    std::string name;
};

struct Stop {
    std::string placeCode;
    std::string description;
};

struct StopDetail {
    float longitude;
    float latitude;
    std::string nextDepartureText;
};

class TransitService {
public:
    TransitService() : ws("https://svc.metrotransit.org") {}
    std::vector<Agency> GetAgencies();
    std::vector<Route> GetRoutes();
    std::vector<Direction> GetDirections(const std::string& routeId);
    std::vector<Stop> GetStops(const std::string& routeId, int direction);
    std::vector<StopDetail> GetStopDetail(const std::string& routeId, int direction, const std::string& placeCode);

private:
    WebService ws;
};

}

#endif