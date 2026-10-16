#include "Routes.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <set>
#include <utility>

#include "json.hpp"

#include "Cache.h"
#include "Config.h"
#include "Fetchers.h"

using json = nlohmann::json;

namespace {
// A tag value as a string, or "" if missing.
std::string tag(const json& tags, const char* key) {
    if (!tags.is_object()) return "";
    json::const_iterator it = tags.find(key);
    if (it == tags.end() || !it->is_string()) return "";
    return it->get<std::string>();
}

bool isMajor(const std::string& highway) {
    return highway == "motorway" || highway == "trunk" || highway == "primary" || highway == "secondary";
}

// One ribbon along a polyline, `lift` meters above the ground.
void addRibbon(gfx::ColorMesh& mesh, const SceneFrame& frame, const std::vector<glm::vec2>& pts, float width,
               float lift, const glm::vec3& color) {
    const int n = int(pts.size());
    if (n < 2) return;
    std::vector<glm::vec3> left(n), right(n);
    glm::vec2 lastDir(1.0f, 0.0f);
    for (int i = 0; i < n; i++) {
        glm::vec2 d = pts[std::min(i + 1, n - 1)] - pts[std::max(i - 1, 0)];
        // A U-turn or repeated point has no direction; keep the last one.
        if (glm::length(d) > 1e-4f) lastDir = glm::normalize(d);
        const glm::vec2 side = glm::vec2(-lastDir.y, lastDir.x) * (width * 0.5f);
        const float z = frame.groundHeight(pts[i].x, pts[i].y) + lift;
        left[i] = glm::vec3(pts[i] - side, z);
        right[i] = glm::vec3(pts[i] + side, z);
    }
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    for (int i = 0; i + 1 < n; i++) {
        mesh.addQuad(left[i], left[i + 1], right[i + 1], right[i], up, color);
    }
}
}

// --- Road classes ------------------------------------------------------------

bool isDrivableRoad(const std::string& highway, const std::string& service, const std::string& area) {
    if (area == "yes") return false;
    static const char* const kRoads[] = {
        "motorway", "trunk", "primary", "secondary", "tertiary", "unclassified", "residential",
        "living_street", "motorway_link", "trunk_link", "primary_link", "secondary_link", "tertiary_link",
    };
    for (size_t i = 0; i < sizeof(kRoads) / sizeof(kRoads[0]); i++) {
        if (highway == kRoads[i]) return true;
    }
    if (highway == "service") {
        return service != "parking_aisle" && service != "driveway" && service != "drive-through";
    }
    return false;
}

float roadWidth(const std::string& highway) {
    if (highway == "motorway") return 12.0f;
    if (highway == "trunk") return 10.0f;
    if (highway == "primary") return 9.0f;
    if (highway == "secondary") return 8.0f;
    if (highway == "tertiary") return 7.0f;
    if (highway == "service") return 4.0f;
    if (highway.size() > 5 && highway.compare(highway.size() - 5, 5, "_link") == 0) return 5.0f;
    return 6.0f;  // residential, unclassified, living_street
}

// --- RouteGraph --------------------------------------------------------------

void RouteGraph::clear() {
    mNodes.clear();
    mEdges.clear();
    mRoads.clear();
    mIndex.clear();
    mRemark.clear();
}

long long RouteGraph::addNode(long long id, const geo::LatLon& pos) {
    if (!mIndex.count(id)) {
        mIndex[id] = mNodes.size();
        Node n = { id, pos };
        mNodes.push_back(n);
    }
    return id;
}

const RouteGraph::Node* RouteGraph::find(long long id) const {
    std::map<long long, size_t>::const_iterator it = mIndex.find(id);
    return it == mIndex.end() ? nullptr : &mNodes[it->second];
}

bool RouteGraph::parse(const std::string& text, std::string& error) {
    clear();
    json doc;
    try {
        doc = json::parse(text);
    } catch (const std::exception&) {
        // nlohmann's exceptions aren't std::runtime_error, so they must not
        // escape to main().
        error = text.empty() ? "the file is empty" : "not valid JSON (an error page?)";
        return false;
    }
    if (!doc.is_object() || doc.find("elements") == doc.end() || !doc["elements"].is_array()) {
        error = "no \"elements\" list (not an Overpass response?)";
        return false;
    }
    if (doc.find("remark") != doc.end() && doc["remark"].is_string()) mRemark = doc["remark"].get<std::string>();

    // `out geom` gives each way its node ids ("nodes") and their positions
    // ("geometry") in the same order. If the ids are missing, nodes at the
    // same position are treated as the same node (OSM roads share nodes).
    std::map<std::pair<double, double>, long long> byPosition;
    long long nextSyntheticId = -1;
    std::set<std::pair<long long, long long>> seen;

    const json& elements = doc["elements"];
    for (json::const_iterator it = elements.begin(); it != elements.end(); ++it) {
        const json& e = *it;
        if (!e.is_object() || tag(e, "type") != "way") continue;
        const json tags = e.find("tags") != e.end() ? e["tags"] : json::object();
        const std::string highway = tag(tags, "highway");
        if (!isDrivableRoad(highway, tag(tags, "service"), tag(tags, "area"))) continue;
        if (e.find("geometry") == e.end() || !e["geometry"].is_array()) continue;

        const json& geometry = e["geometry"];
        const json* ids = (e.find("nodes") != e.end() && e["nodes"].is_array() &&
                           e["nodes"].size() == geometry.size()) ? &e["nodes"] : nullptr;

        Road road;
        road.id = (e.find("id") != e.end() && e["id"].is_number()) ? e["id"].get<long long>() : 0;
        road.highway = highway;
        for (size_t k = 0; k < geometry.size(); k++) {
            const json& p = geometry[k];
            if (!p.is_object() || p.find("lat") == p.end() || p.find("lon") == p.end() ||
                !p["lat"].is_number() || !p["lon"].is_number()) {
                // A point Overpass left out: end this piece of the road here.
                if (road.nodes.size() >= 2) mRoads.push_back(road);
                road.nodes.clear();
                continue;
            }
            const geo::LatLon pos = { p["lat"].get<double>(), p["lon"].get<double>() };
            long long id;
            if (ids && (*ids)[k].is_number()) {
                id = (*ids)[k].get<long long>();
            } else {
                const std::pair<double, double> key(pos.lat, pos.lon);
                std::map<std::pair<double, double>, long long>::iterator found = byPosition.find(key);
                id = found != byPosition.end() ? found->second : (byPosition[key] = nextSyntheticId--);
            }
            addNode(id, pos);
            if (!road.nodes.empty() && road.nodes.back() != id) {
                const std::pair<long long, long long> key(std::min(road.nodes.back(), id),
                                                          std::max(road.nodes.back(), id));
                if (seen.insert(key).second) {
                    Edge edge = { road.nodes.back(), id };
                    mEdges.push_back(edge);
                }
            }
            if (road.nodes.empty() || road.nodes.back() != id) road.nodes.push_back(id);
        }
        if (road.nodes.size() >= 2) mRoads.push_back(road);
    }
    return true;
}

bool RouteGraph::load(const geo::LatLonBox& scene, std::string& note) {
    clear();
    note.clear();

    std::string error;
    const datafiles::Match match = datafiles::find(config::kOsmDir, "roads", ".json", scene);
    if (!match.path.empty()) {
        if (!parse(tiles::readFile(match.path), error)) {
            clear();
            note = "Can't read " + match.path + ": " + error;
            return false;
        }
        if (!match.full) note = "Roads file covers only part of the scene.";
    } else {
        const geo::LatLonBox request = scene.paddedAndRounded(config::kFetchPadding);
        std::string fetchError;
        const std::string body = fetchOsmRoads(request, fetchError);
        if (body.empty()) {
            note = "No roads for this scene (" + fetchError + "). Download " + request.describe() +
                   " - see README.md.";
            return false;
        }
        if (!parse(body, error)) {
            clear();
            note = "Downloaded roads aren't valid: " + error;
            return false;
        }
        // An incomplete response isn't cached, so the next try downloads again.
        if (mRemark.empty()) {
            tiles::writeFile(datafiles::pathFor(config::kOsmDir, "roads", "", request, ".json"), body);
        }
    }

    if (!mRemark.empty()) {
        if (!note.empty()) note += "\n";
        note += "Roads data is incomplete. Overpass said: " + mRemark.substr(0, 80);
    }
    return true;
}

// --- Drawing -----------------------------------------------------------------

int buildRoadMesh(gfx::ColorMesh& mesh, const SceneFrame& frame, const RouteGraph& graph) {
    // Sit above the terrain mesh; its cells are coarser than the roads,
    // and exaggeration magnifies the difference.
    const float lift = frame.size() * 0.0015f * std::max(1.0f, frame.exaggeration);
    // Short steps so the ribbons follow the terrain between nodes.
    const float step = std::max(5.0f, frame.size() / 400.0f);
    const glm::vec3 majorColor(1.0f, 0.55f, 0.1f);
    const glm::vec3 minorColor(1.0f, 0.78f, 0.4f);

    int pieces = 0;
    const std::vector<RouteGraph::Road>& roads = graph.roads();
    for (size_t r = 0; r < roads.size(); r++) {
        const RouteGraph::Road& road = roads[r];
        const float width = roadWidth(road.highway);
        const glm::vec3 color = isMajor(road.highway) ? majorColor : minorColor;

        // Split the road into runs of segments that touch the scene.
        std::vector<glm::vec2> run;
        for (size_t k = 0; k + 1 < road.nodes.size(); k++) {
            const glm::vec2 a(frame.toLocal(graph.find(road.nodes[k])->pos));
            const glm::vec2 b(frame.toLocal(graph.find(road.nodes[k + 1])->pos));
            if (!frame.inside(a.x, a.y) && !frame.inside(b.x, b.y)) {
                if (run.size() >= 2) { addRibbon(mesh, frame, run, width, lift, color); pieces++; }
                run.clear();
                continue;
            }
            const float length = glm::distance(a, b);
            if (length < 0.01f) continue;
            if (run.empty()) run.push_back(a);
            const int n = std::max(1, int(std::ceil(length / step)));
            for (int s = 1; s <= n; s++) run.push_back(glm::mix(a, b, float(s) / n));
        }
        if (run.size() >= 2) { addRibbon(mesh, frame, run, width, lift, color); pieces++; }
    }
    return pieces;
}

void RoutesLayer::load(const SceneFrame& frame) {
    mMesh.clear();
    if (!mTried || mTriedFor != frame.latLon) {
        mTried = true;
        mTriedFor = frame.latLon;
        mGraphOk = mGraph.load(frame.latLon, mGraphNote);
    }
    note = mGraphNote;
    if (!mGraphOk) return;

    buildRoadMesh(mMesh, frame, mGraph);
    mMesh.upload(GL_TRIANGLES);
}

void RoutesLayer::draw(const glm::mat4& viewProj) {
    mShader.use(viewProj);
    mMesh.draw();
}
