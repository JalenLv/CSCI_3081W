#ifndef MAPVIEWER_GEO_H_
#define MAPVIEWER_GEO_H_

// Web Mercator helpers and a lat/lon bounding box. Mercator coordinates are
// in [0,1]: x grows east, y grows SOUTH (same order as tile rows).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

namespace geo {
const double kPi = 3.14159265358979323846;
const double kMaxLat = 85.0511287798;            // Mercator's limit
const double kEarthCircumference = 40075016.686; // meters at the equator

struct LatLon { double lat; double lon; };
struct MercBounds { double minX, minY, maxX, maxY; };

inline double lonToMercX(double lon) { return (lon + 180.0) / 360.0; }
inline double latToMercY(double lat) {
    double r = lat * kPi / 180.0;
    return (kPi - std::log(std::tan(kPi / 4.0 + r / 2.0))) / (2.0 * kPi);
}
inline double mercXToLon(double x) { return 360.0 * x - 180.0; }
inline double mercYToLat(double y) {
    return (2.0 * std::atan(std::exp(kPi - 2.0 * kPi * y)) - kPi / 2.0) * 180.0 / kPi;
}

inline MercBounds clampToWorld(const MercBounds& b) {
    return { std::max(b.minX, 0.0), std::max(b.minY, 0.0),
             std::min(b.maxX, 1.0), std::min(b.maxY, 1.0) };
}

// Tile index containing Mercator coordinate m at a zoom with n tiles per side.
inline int tileIndex(double m, int n) {
    return std::min(std::max((int)std::floor(m * n), 0), n - 1);
}

inline std::string format(const LatLon& p) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.5f, %.5f", p.lat, p.lon);
    return buf;
}

// Accepts "lat, lon" or "lat lon" (e.g. pasted from Google Maps).
inline bool parse(const std::string& text, LatLon& out) {
    std::string s = text;
    std::replace(s.begin(), s.end(), ',', ' ');
    std::istringstream in(s);
    double lat, lon;
    if (!(in >> lat >> lon)) return false;
    std::string extra;
    if (in >> extra) return false;
    if (lat < -kMaxLat || lat > kMaxLat || lon < -180.0 || lon > 180.0) return false;
    out.lat = lat;
    out.lon = lon;
    return true;
}

// A box in degrees. Areas below are in square degrees, which is fine for
// comparing boxes that sit next to each other.
struct LatLonBox {
    double south = 0.0, west = 0.0, north = 0.0, east = 0.0;

    LatLonBox() {}
    LatLonBox(double s, double w, double n, double e) : south(s), west(w), north(n), east(e) {}

    bool valid() const { return south < north && west < east; }
    bool operator==(const LatLonBox& o) const {
        return south == o.south && west == o.west && north == o.north && east == o.east;
    }
    bool operator!=(const LatLonBox& o) const { return !(*this == o); }
    double area() const { return valid() ? (north - south) * (east - west) : 0.0; }

    bool contains(const LatLonBox& o) const {
        return o.south >= south && o.north <= north && o.west >= west && o.east <= east;
    }
    bool contains(double lat, double lon) const {
        return lat >= south && lat <= north && lon >= west && lon <= east;
    }

    LatLonBox intersect(const LatLonBox& o) const {
        return LatLonBox(std::max(south, o.south), std::max(west, o.west),
                         std::min(north, o.north), std::min(east, o.east));
    }

    // Grown by `fraction` of its size on every side, then rounded outward to
    // 0.001 degrees so file names stay short.
    LatLonBox paddedAndRounded(double fraction) const {
        const double dLat = (north - south) * fraction, dLon = (east - west) * fraction;
        return LatLonBox(std::floor((south - dLat) * 1000.0) / 1000.0,
                         std::floor((west - dLon) * 1000.0) / 1000.0,
                         std::ceil((north + dLat) * 1000.0) / 1000.0,
                         std::ceil((east + dLon) * 1000.0) / 1000.0);
    }

    // "S_W_N_E" with 3 decimals, as used in data file names.
    std::string fileToken() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%.3f_%.3f_%.3f_%.3f", south, west, north, east);
        return buf;
    }

    // "S, W, N, E" for messages.
    std::string describe() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "S %.3f, W %.3f, N %.3f, E %.3f", south, west, north, east);
        return buf;
    }
};

// Lat/lon box of a Web Mercator area. Remember y grows south: the north edge
// is minY.
inline LatLonBox toLatLonBox(const MercBounds& b) {
    return LatLonBox(mercYToLat(b.maxY), mercXToLon(b.minX), mercYToLat(b.minY), mercXToLon(b.maxX));
}
}

#endif
