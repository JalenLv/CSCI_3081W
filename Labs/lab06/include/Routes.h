#ifndef MAPVIEWER_ROUTES_H_
#define MAPVIEWER_ROUTES_H_

// Route graph built from OpenStreetMap roads: an Overpass API response
// (`out geom` JSON) stored in apps/MapViewer/data/osm as
// roads_<S>_<W>_<N>_<E>.json (see README.md, 5.3 and 6.4).
//
// Nodes are OSM nodes (real OSM ids, lat/lon); edges join consecutive nodes
// along each road, so curves keep their shape. RouteGraph has no GL, so
// routing code can use it on its own. RoutesLayer draws the roads as ribbons
// on the ground.

#include <map>
#include <string>
#include <vector>

#include "Geo.h"
#include "Graphics.h"
#include "Scene.h"

class RouteGraph {
public:
    struct Node { long long id; geo::LatLon pos; };
    struct Edge { long long from, to; };               // undirected for now
    struct Road { long long id; std::string highway; std::vector<long long> nodes; };

    // Loads the roads for `scene` from the best roads_*.json in
    // config::kOsmDir, or from fetchOsmRoads() if no file overlaps the scene.
    // Returns false when there's nothing usable. `note` gets a message for
    // the HUD; it can be set on success too (partial coverage, incomplete
    // response).
    bool load(const geo::LatLonBox& scene, std::string& note);

    // Parses Overpass JSON, keeping drivable roads only (see isDrivableRoad).
    // Public for testing.
    bool parse(const std::string& text, std::string& error);
    void clear();

    const std::vector<Node>& nodes() const { return mNodes; }
    const std::vector<Edge>& edges() const { return mEdges; }
    const std::vector<Road>& roads() const { return mRoads; }
    const Node* find(long long id) const;  // nullptr if unknown
    const std::string& remark() const { return mRemark; }  // Overpass "remark" (e.g. timeout)

private:
    long long addNode(long long id, const geo::LatLon& pos);

    std::vector<Node> mNodes;
    std::vector<Edge> mEdges;
    std::vector<Road> mRoads;
    std::map<long long, size_t> mIndex;  // node id -> position in mNodes
    std::string mRemark;
};

// True for the roads the graph keeps: public road classes, plus service
// roads that aren't driveways or parking aisles; never area=yes.
// Tag values are "" when absent. Matches the Overpass query in the README.
bool isDrivableRoad(const std::string& highway, const std::string& service, const std::string& area);

// Drawn ribbon width in meters for a highway class.
float roadWidth(const std::string& highway);

// Adds the roads inside the scene to `mesh` as ribbons that follow the
// ground. Returns the number of road pieces drawn.
int buildRoadMesh(gfx::ColorMesh& mesh, const SceneFrame& frame, const RouteGraph& graph);

class RoutesLayer : public MapLayer {
public:
    explicit RoutesLayer(const gfx::ColorShader& shader) : mShader(shader) {}
    std::string name() const override { return "Routes"; }

    // Reads the data once per scene; later calls (terrain changes) only
    // rebuild the mesh.
    void load(const SceneFrame& frame) override;
    void unload() override { mMesh.clear(); }
    void draw(const glm::mat4& viewProj) override;

    const RouteGraph& graph() const { return mGraph; }

private:
    const gfx::ColorShader& mShader;
    gfx::ColorMesh mMesh;

    RouteGraph mGraph;
    bool mTried = false;          // tried to load data for mTriedFor
    geo::LatLonBox mTriedFor;
    bool mGraphOk = false;
    std::string mGraphNote;
};

#endif
