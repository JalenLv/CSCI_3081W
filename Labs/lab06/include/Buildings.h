#ifndef MAPVIEWER_BUILDINGS_H_
#define MAPVIEWER_BUILDINGS_H_

// OpenStreetMap buildings from an Overpass API response (`out geom` JSON),
// stored in apps/MapViewer/data/osm (see README.md, 5.2 and 6.3).
//
// BuildingData parses the file into footprints in lat/lon and has no GL, so
// it can be used on its own. BuildingsLayer extrudes the footprints in the
// 3D scene.

#include <cstdint>
#include <string>
#include <vector>

#include "Geo.h"
#include "Graphics.h"
#include "Scene.h"

// One building outline in degrees, with optional courtyards.
struct Footprint {
    std::vector<geo::LatLon> outer;               // ring, first point not repeated at the end
    std::vector<std::vector<geo::LatLon>> holes;  // courtyards (inner rings)
    float height = 0.0f;                          // meters, see buildingHeight()
    long long id = 0;                             // OSM way or relation id
    bool fromRelation = false;
};

class BuildingData {
public:
    // Loads the buildings for `scene` from the best file in config::kOsmDir,
    // or from fetchOsmBuildings() if no file overlaps the scene. Returns false
    // when there's nothing usable. `note` gets a message for the HUD; it can
    // be set on success too (partial coverage, incomplete response).
    bool load(const geo::LatLonBox& scene, std::string& note);

    // Parses Overpass JSON. Public for testing.
    bool parse(const std::string& text, std::string& error);

    void clear();

    const std::vector<Footprint>& footprints() const { return mFootprints; }
    int skipped() const { return mSkipped; }               // outlines that couldn't be closed
    const std::string& remark() const { return mRemark; }  // Overpass "remark" (e.g. timeout)

private:
    std::vector<Footprint> mFootprints;
    int mSkipped = 0;
    std::string mRemark;
};

// Building height in meters from the OSM `height` and `building:levels` tag
// values (empty if absent): height, else levels x config::kMetersPerLevel,
// else config::kDefaultBuildingHeight. See README.md, 6.3.
float buildingHeight(const std::string& heightTag, const std::string& levelsTag);

// "12", "12 m", "40'", "40 ft", "40'6\"", "12;15" -> meters. False if unreadable.
bool parseHeightTag(const std::string& text, float& meters);

// Triangulates a polygon given as rings (first = outline, rest = holes) with
// ear clipping. Returns indices into the rings' points taken in order, three
// per triangle; empty if it fails.
std::vector<uint32_t> triangulate(const std::vector<std::vector<glm::vec2>>& rings);

// Adds one extruded building (flat roof + walls) to `mesh`, standing on the
// lowest ground under its outline. Returns false if it's outside the scene or
// can't be triangulated.
bool extrudeFootprint(gfx::ColorMesh& mesh, const SceneFrame& frame, const Footprint& fp,
                      const glm::vec3& color);

class BuildingsLayer : public MapLayer {
public:
    explicit BuildingsLayer(const gfx::ColorShader& shader) : mShader(shader) {}
    std::string name() const override { return "Buildings"; }

    // Reads the data once per scene; later calls (terrain changes) only
    // rebuild the mesh.
    void load(const SceneFrame& frame) override;
    void unload() override { mMesh.clear(); }
    void draw(const glm::mat4& viewProj) override;

    const BuildingData& data() const { return mData; }

private:
    const gfx::ColorShader& mShader;
    gfx::ColorMesh mMesh;

    BuildingData mData;
    bool mTried = false;          // tried to load data for mTriedFor
    geo::LatLonBox mTriedFor;
    bool mDataOk = false;
    std::string mDataNote;
};

#endif
