#ifndef WEB_SERVICE_H_
#define WEB_SERVICE_H_

#include <string>
#include "json.hpp"
using json = nlohmann::json;

class WebService {
public:
    WebService(const std::string& api);
    std::string Get(const std::string& route) const;
    json GetJSON(const std::string& route) const;

private:
    std::string api;
};

#endif