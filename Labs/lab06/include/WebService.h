#ifndef WEB_SERVICE_H_
#define WEB_SERVICE_H_

#include <string>
#include "json.hpp"
using json = nlohmann::json;

namespace gis {

class WebService {
public:
    WebService(const std::string& api);
    std::string get(const std::string& route) const;
    json getJson(const std::string& route) const;

private:
    std::string api;
};

}

#endif