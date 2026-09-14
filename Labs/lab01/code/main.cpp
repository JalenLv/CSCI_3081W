#include <iostream> // Used for io (e.g. cout, cin)
#include <string> // Used for string functions
#include <cmath> // Used for mathematical functions

#include "download.h"
#include "map.h"

// Converts longitude to Mercator coordinates
double longitudeToMercator(double longitude) {
  return (180.0+longitude)/360.0;
}

// Converts latitude to Mercator coordinates
double latitudeToMercator(double latitude) {
  return (1.0/(2.0*M_PI)*(M_PI-std::log(std::tan(M_PI/4 + latitude*M_PI/180.0/2.0))));
}


int main(int argc, char* argv[]) {
  if (argc < 4) {
    std::cout << "Usage: ./map_zoom <service> <start zoom level> <num zoom levels>" << std::endl;
    std::cout << "Example: ./map_zoom World_Imagery 0 20" << std::endl;

    // Download the available services from the web:
    std::cout << "\nAvailable Services:" << std::endl;
    std::cout << download("https://server.arcgisonline.com/arcgis/rest/services/?f=pjson") << std::endl;
    exit(0);
  }

  // Parse args
  std::string service = argv[1];
  int startZoom = std::stoi(argv[2]);
  int numZoomLevels = std::stoi(argv[3]);
  int endingZoom = startZoom + numZoomLevels - 1;
  
  std::cout << "Requesting map tiles for " << service
            << " and zoom levels from " << startZoom
            << " to " << endingZoom << std::endl;

  double longIn, latIn;
  std::cout << "Enter a Longitude: ";
  std::cin >> longIn;
  std::cout << "Enter a Latitude: ";
  std::cin >> latIn;

  double x = longitudeToMercator(longIn);
  double y = latitudeToMercator(latIn);

  // Lab (Mercator coordinates between 0 and 1):
  // float x = 0.2410205;
  // float y = 0.359827;

  // Minneapolis 44.9778° N, 93.2650 W
  // float x = longitudeToMercator(-93.26384);
  // float y = latitudeToMercator(44.9778);

  for (int zoom = startZoom; zoom <= endingZoom; zoom++) {
    std::string outputImageFile = std::to_string(zoom) + ".png";
    int tileX = std::floor(x * std::pow(2, zoom));
    int tileY = std::floor(y * std::pow(2, zoom));
    download_map_image(outputImageFile.c_str(), zoom, tileX, tileY, service.c_str());
  }

  return 0;
}
