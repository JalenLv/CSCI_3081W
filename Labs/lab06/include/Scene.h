#ifndef MAPVIEWER_SCENE_H_
#define MAPVIEWER_SCENE_H_

// The frozen 3D scene (SceneFrame) and the interface for objects drawn in it
// (MapLayer). No GL calls here.

#include <algorithm>
#include <cmath>
#include <string>

#include <glm/glm.hpp>

#include "Elevation.h"
#include "Geo.h"

// ---------------------------------------------------------------------------
// The frozen 3D scene: local meters around an origin
//
// x = meters east, y = meters north, z = meters up, relative to the center
// of the rendered area. Web Mercator distances are scaled by cos(origin
// latitude) so sizes are true meters (accurate over a city-sized area).
// Ground z is (terrain height - lowest terrain in the scene) * exaggeration,
// so the scene sits near z = 0 and exaggeration stretches relief only.
// ---------------------------------------------------------------------------

struct SceneFrame {
    geo::MercBounds coverage;   // rendered tile area (Mercator)
    geo::LatLonBox latLon;      // the same area in degrees
    glm::dvec2 originMerc;
    double metersPerMerc = 1.0; // local meters per Web Mercator unit
    int zoom = 0;               // tile zoom the scene was rendered at
    glm::vec2 minLocal, maxLocal;

    const ElevationGrid* elevation = nullptr;
    bool useElevation = false;
    float exaggeration = 1.0f;

    void build(const geo::MercBounds& cov, int tileZoom) {
        coverage = cov;
        latLon = geo::toLatLonBox(cov);
        zoom = tileZoom;
        originMerc = glm::dvec2((cov.minX + cov.maxX) / 2.0, (cov.minY + cov.maxY) / 2.0);
        double originLat = geo::mercYToLat(originMerc.y) * geo::kPi / 180.0;
        metersPerMerc = geo::kEarthCircumference * std::cos(originLat);
        minLocal = glm::vec2(float((cov.minX - originMerc.x) * metersPerMerc),
                             float(-(cov.maxY - originMerc.y) * metersPerMerc));
        maxLocal = glm::vec2(float((cov.maxX - originMerc.x) * metersPerMerc),
                             float(-(cov.minY - originMerc.y) * metersPerMerc));
    }

    glm::dvec2 localToMerc(float x, float y) const {
        return glm::dvec2(originMerc.x + x / metersPerMerc, originMerc.y - y / metersPerMerc);
    }

    // Local x/y (z = elevation argument) for a lat/lon.
    glm::vec3 toLocal(const geo::LatLon& p, double z = 0.0) const {
        return glm::vec3(float((geo::lonToMercX(p.lon) - originMerc.x) * metersPerMerc),
                         float(-(geo::latToMercY(p.lat) - originMerc.y) * metersPerMerc),
                         float(z));
    }

    geo::LatLon toLatLon(const glm::vec3& p) const {
        glm::dvec2 m = localToMerc(p.x, p.y);
        return { geo::mercYToLat(m.y), geo::mercXToLon(m.x) };
    }

    bool inside(float x, float y) const {
        return x >= minLocal.x && x <= maxLocal.x && y >= minLocal.y && y <= maxLocal.y;
    }

    bool hasTerrain() const { return useElevation && elevation && elevation->loaded(); }

    // Real terrain height above sea level in meters (0 without terrain).
    float elevationAt(float x, float y) const {
        if (!hasTerrain()) return 0.0f;
        glm::dvec2 m = localToMerc(x, y);
        return elevation->sample(m.x, m.y);
    }

    // Scene z of the ground at local x/y. Layers should place objects on this.
    float groundHeight(float x, float y) const {
        if (!hasTerrain()) return 0.0f;
        return (elevationAt(x, y) - elevation->minHeight()) * exaggeration;
    }

    float size() const {
        return std::max(maxLocal.x - minLocal.x, maxLocal.y - minLocal.y);
    }
};

// ---------------------------------------------------------------------------
// 3D layers
//
// Interface for routes / buildings / other objects. load() is called with
// the frozen SceneFrame when the layer is first needed in 3D view (and again
// whenever the terrain changes); place geometry on frame.groundHeight(x, y).
// Layers should keep their parsed data between load() calls for the same
// scene, so a terrain change only rebuilds geometry. draw() gets the
// camera's projection * view matrix (local meters).
// ---------------------------------------------------------------------------

class MapLayer {
public:
    virtual ~MapLayer() {}
    virtual std::string name() const = 0;
    virtual void load(const SceneFrame& frame) = 0;
    virtual void unload() = 0;
    virtual void draw(const glm::mat4& viewProj) = 0;

    bool visible = true;  // checkbox state
    bool loaded = false;  // managed by the app
    std::string note;     // set by load(): a problem to show in the HUD, or ""
};

#endif
