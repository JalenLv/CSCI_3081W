#include <iostream>
#include <cstdio>
#include "TransitService.h"

using namespace MetroTransitAPI;

void listAgencies(TransitService& service);
void listRoutes(TransitService& service);
void realTimeInfo(TransitService& service);

int main() {
    TransitService service = TransitService();

    int input = -1;

    while (input != 0) {
        std::cout << "\nOptions:\n-------------------" << std::endl;
        std::cout << "0: Exit" << std::endl;
        std::cout << "1: List Agencies" << std::endl;
        std::cout << "2: List Routes" << std::endl;
        std::cout << "3: Real-Time Info" << std::endl;
        std::cout << "\nEnter a selection: ";
        std::cin >> input;
        std::cout << std::endl;

        switch(input) {
            case 0:
                break;
            case 1:
                listAgencies(service);
                break;
            case 2:
                listRoutes(service);
                break;
            case 3:
                realTimeInfo(service);
                break;
        }

        if (input != 0) {
            std::cout << "\n<Press enter to continue>";
            getchar();
            getchar();
        }

    }
}

void listAgencies(TransitService& service) {
    std::vector<Agency> agencies = service.GetAgencies();
    for (unsigned int i = 0; i < agencies.size(); i++) {
        std::cout << agencies[i].id  << ": " << agencies[i].name << std::endl;
    }
}

void listRoutes(TransitService& service) {
    std::vector<Route> routes = service.GetRoutes();
    for (unsigned int i = 0; i < routes.size(); i++) {
        std::cout << routes[i].id  << ": " << routes[i].name << std::endl;
    }
}

// **************************** Milestone 3 ****************************
// Implement the TODO's in the following function
void realTimeInfo(TransitService& service) {
    std::cout << "Enter a route: ";
    std::string routeId;
    std::cin >> routeId;
    std::cout << std::endl;

    // TODO: list directions

    int dirId;
    std::cout << "Enter a direction: ";
    std::cin >> dirId;
    std::cout << std::endl;

    // TODO: list stops

    std::string code;
    std::cout << "Enter a stop: ";
    std::cin >> code;
    std::cout << std::endl;

    // TODO: Output location information along with the next scheduled trip
}