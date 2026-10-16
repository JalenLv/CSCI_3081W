#include "Buildings.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <limits>

#include "json.hpp"
#include "earcut.hpp"

#include "Cache.h"
#include "Config.h"
#include "Fetchers.h"

using json = nlohmann::json;

namespace {
typedef std::vector<geo::LatLon> Line;

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = char(std::tolower((unsigned char)s[i]));
    return s;
}

// First value of a ";"-separated OSM tag, with "," accepted as a decimal point.
std::string firstValue(const std::string& tag) {
    std::string s = trim(tag.substr(0, tag.find(';')));
    std::replace(s.begin(), s.end(), ',', '.');
    return s;
}

// A tag value as a string, or "" if missing.
std::string tag(const json& tags, const char* key) {
    if (!tags.is_object()) return "";
    json::const_iterator it = tags.find(key);
    if (it == tags.end() || !it->is_string()) return "";
    return it->get<std::string>();
}

// Points of an Overpass `geometry` array. Null entries (which Overpass uses
// for points it left out) are skipped.
Line readGeometry(const json& geometry) {
    Line line;
    if (!geometry.is_array()) return line;
    for (json::const_iterator it = geometry.begin(); it != geometry.end(); ++it) {
        const json& p = *it;
        if (!p.is_object() || p.find("lat") == p.end() || p.find("lon") == p.end()) continue;
        if (!p["lat"].is_number() || !p["lon"].is_number()) continue;
        geo::LatLon ll = { p["lat"].get<double>(), p["lon"].get<double>() };
        line.push_back(ll);
    }
    return line;
}

// OSM shares node coordinates between ways, so endpoints compare exactly.
bool samePoint(const geo::LatLon& a, const geo::LatLon& b) {
    return a.lat == b.lat && a.lon == b.lon;
}

// Joins way pieces end to end into closed rings (multipolygon members are
// often split into several ways). Rings come back without the repeated
// closing point. Pieces that never close are counted in `broken`.
std::vector<Line> joinRings(std::vector<Line> pieces, int& broken) {
    std::vector<Line> rings;
    while (!pieces.empty()) {
        Line ring = pieces.back();
        pieces.pop_back();
        if (ring.empty()) continue;

        while (!samePoint(ring.front(), ring.back())) {
            bool joined = false;
            for (size_t i = 0; i < pieces.size() && !joined; i++) {
                const Line& l = pieces[i];
                if (l.size() < 2) continue;
                if (samePoint(l.front(), ring.back())) {
                    ring.insert(ring.end(), l.begin() + 1, l.end());
                } else if (samePoint(l.back(), ring.back())) {
                    ring.insert(ring.end(), l.rbegin() + 1, l.rend());
                } else if (samePoint(l.back(), ring.front())) {
                    ring.insert(ring.begin(), l.begin(), l.end() - 1);
                } else if (samePoint(l.front(), ring.front())) {
                    ring.insert(ring.begin(), l.rbegin(), l.rend() - 1);
                } else {
                    continue;
                }
                pieces.erase(pieces.begin() + long(i));
                joined = true;
            }
            if (!joined) break;
        }

        if (ring.size() >= 4 && samePoint(ring.front(), ring.back())) {
            ring.pop_back();
            rings.push_back(ring);
        } else {
            broken++;
        }
    }
    return rings;
}

// Even-odd point-in-polygon test in degrees (fine at city scale).
bool insideRing(const geo::LatLon& p, const Line& ring) {
    bool in = false;
    for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const geo::LatLon& a = ring[i];
        const geo::LatLon& b = ring[j];
        if ((a.lat > p.lat) != (b.lat > p.lat) &&
            p.lon < (b.lon - a.lon) * (p.lat - a.lat) / (b.lat - a.lat) + a.lon) {
            in = !in;
        }
    }
    return in;
}

bool isBuilding(const json& tags) {
    const std::string b = tag(tags, "building");
    return !b.empty() && b != "no";
}

double signedArea(const std::vector<glm::vec2>& ring) {
    double a = 0.0;
    for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        a += double(ring[j].x) * ring[i].y - double(ring[i].x) * ring[j].y;
    }
    return a / 2.0;
}

// Footprint ring in local meters, without repeated points, with the given
// winding (counter-clockwise for outlines, clockwise for holes).
std::vector<glm::vec2> localRing(const SceneFrame& frame, const Line& ring, bool counterClockwise) {
    std::vector<glm::vec2> out;
    out.reserve(ring.size());
    for (size_t i = 0; i < ring.size(); i++) {
        const glm::vec3 p = frame.toLocal(ring[i]);
        const glm::vec2 q(p.x, p.y);
        if (out.empty() || glm::distance(out.back(), q) > 0.01f) out.push_back(q);
    }
    while (out.size() > 1 && glm::distance(out.front(), out.back()) <= 0.01f) out.pop_back();
    if (out.size() < 3) return std::vector<glm::vec2>();
    if ((signedArea(out) > 0.0) != counterClockwise) std::reverse(out.begin(), out.end());
    return out;
}

// Stable per-building shade so neighbors are easy to tell apart.
glm::vec3 buildingColor(long long id) {
    unsigned long long h = (unsigned long long)id * 0x9E3779B97F4A7C15ull;
    const float t = float((h >> 40) % 1000) / 1000.0f;
    const float shade = 0.7f + 0.25f * t;
    return glm::vec3(shade * 0.85f, shade * 0.88f, shade);
}
}

// --- Heights ---------------------------------------------------------------

bool parseHeightTag(const std::string& text, float& meters) {
    const std::string s = firstValue(text);
    const char* start = s.c_str();
    char* end = nullptr;
    const double v = std::strtod(start, &end);
    if (end == start) return false;
    const std::string unit = lower(trim(end));

    double m;
    if (unit.empty() || unit == "m" || unit == "meter" || unit == "meters" || unit == "metre" || unit == "metres") {
        m = v;
    } else if (unit == "ft" || unit == "feet" || unit == "'") {
        m = v * 0.3048;
    } else if (unit[0] == '\'') {
        // 40'6" = 40 feet 6 inches
        const std::string rest = trim(unit.substr(1));
        char* inEnd = nullptr;
        const double inches = std::strtod(rest.c_str(), &inEnd);
        const std::string tail = trim(inEnd);
        if (inEnd == rest.c_str() || !(tail == "\"" || tail == "in" || tail.empty())) return false;
        m = v * 0.3048 + inches * 0.0254;
    } else {
        return false;
    }
    if (!(m > 0.0 && m < 1000.0)) return false;
    meters = float(m);
    return true;
}

float buildingHeight(const std::string& heightTag, const std::string& levelsTag) {
    float h = 0.0f;
    if (!heightTag.empty() && parseHeightTag(heightTag, h)) return h;

    if (!levelsTag.empty()) {
        const std::string s = firstValue(levelsTag);
        char* end = nullptr;
        const double levels = std::strtod(s.c_str(), &end);
        if (end != s.c_str() && trim(end).empty() && levels > 0.0 && levels < 300.0) {
            return float(levels) * config::kMetersPerLevel;
        }
    }
    return config::kDefaultBuildingHeight;
}

// --- BuildingData ------------------------------------------------------------

void BuildingData::clear() {
    mFootprints.clear();
    mSkipped = 0;
    mRemark.clear();
}

bool BuildingData::parse(const std::string& text, std::string& error) {
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

    const json& elements = doc["elements"];
    for (json::const_iterator it = elements.begin(); it != elements.end(); ++it) {
        const json& e = *it;
        if (!e.is_object() || e.find("type") == e.end() || !e["type"].is_string()) continue;
        const std::string type = e["type"].get<std::string>();
        const json tags = e.find("tags") != e.end() ? e["tags"] : json::object();
        if (!isBuilding(tags)) continue;  // e.g. untagged member ways, building=no

        const long long id = (e.find("id") != e.end() && e["id"].is_number()) ? e["id"].get<long long>() : 0;
        const float height = buildingHeight(tag(tags, "height"), tag(tags, "building:levels"));

        if (type == "way") {
            Line ring = e.find("geometry") != e.end() ? readGeometry(e["geometry"]) : Line();
            if (ring.size() < 4 || !samePoint(ring.front(), ring.back())) {
                mSkipped++;
                continue;
            }
            ring.pop_back();
            Footprint fp;
            fp.outer = ring;
            fp.height = height;
            fp.id = id;
            mFootprints.push_back(fp);
        } else if (type == "relation") {
            if (tag(tags, "type") != "multipolygon") continue;
            if (e.find("members") == e.end() || !e["members"].is_array()) continue;

            std::vector<Line> outerPieces, innerPieces;
            const json& members = e["members"];
            for (json::const_iterator m = members.begin(); m != members.end(); ++m) {
                if (!m->is_object() || tag(*m, "type") != "way") continue;
                if (m->find("geometry") == m->end()) continue;
                const std::string role = (m->find("role") != m->end() && (*m)["role"].is_string())
                                             ? (*m)["role"].get<std::string>() : "";
                Line piece = readGeometry((*m)["geometry"]);
                if (piece.size() < 2) continue;
                if (role == "inner") innerPieces.push_back(piece);
                else outerPieces.push_back(piece);  // "outer" or unset
            }

            int broken = 0;
            const std::vector<Line> outers = joinRings(outerPieces, broken);
            const std::vector<Line> inners = joinRings(innerPieces, broken);
            mSkipped += broken;

            const size_t first = mFootprints.size();
            for (size_t i = 0; i < outers.size(); i++) {
                Footprint fp;
                fp.outer = outers[i];
                fp.height = height;
                fp.id = id;
                fp.fromRelation = true;
                mFootprints.push_back(fp);
            }
            // Each courtyard goes to the outline that contains it.
            for (size_t h = 0; h < inners.size(); h++) {
                for (size_t i = first; i < mFootprints.size(); i++) {
                    if (insideRing(inners[h][0], mFootprints[i].outer)) {
                        mFootprints[i].holes.push_back(inners[h]);
                        break;
                    }
                }
            }
        }
    }
    return true;
}

bool BuildingData::load(const geo::LatLonBox& scene, std::string& note) {
    clear();
    note.clear();

    std::string error;
    const datafiles::Match match = datafiles::find(config::kOsmDir, "buildings", ".json", scene);
    if (!match.path.empty()) {
        if (!parse(tiles::readFile(match.path), error)) {
            clear();
            note = "Can't read " + match.path + ": " + error;
            return false;
        }
        if (!match.full) note = "Buildings file covers only part of the scene.";
    } else {
        const geo::LatLonBox request = scene.paddedAndRounded(config::kFetchPadding);
        std::string fetchError;
        const std::string body = fetchOsmBuildings(request, fetchError);
        if (body.empty()) {
            note = "No buildings for this scene (" + fetchError + "). Download " + request.describe() +
                   " - see README.md.";
            return false;
        }
        if (!parse(body, error)) {
            clear();
            note = "Downloaded buildings aren't valid: " + error;
            return false;
        }
        // An incomplete response isn't cached, so the next try downloads again.
        if (mRemark.empty()) {
            tiles::writeFile(datafiles::pathFor(config::kOsmDir, "buildings", "", request, ".json"), body);
        }
    }

    if (!mRemark.empty()) {
        if (!note.empty()) note += "\n";
        note += "Buildings data is incomplete. Overpass said: " + mRemark.substr(0, 80);
    }
    return true;
}

// --- Triangulation and extrusion ---------------------------------------------

std::vector<uint32_t> triangulate(const std::vector<std::vector<glm::vec2>>& rings) {
    std::vector<std::vector<std::array<double, 2>>> polygon(rings.size());
    for (size_t r = 0; r < rings.size(); r++) {
        polygon[r].reserve(rings[r].size());
        for (size_t i = 0; i < rings[r].size(); i++) {
            std::array<double, 2> p = {{ rings[r][i].x, rings[r][i].y }};
            polygon[r].push_back(p);
        }
    }
    return mapbox::earcut<uint32_t>(polygon);
}

bool extrudeFootprint(gfx::ColorMesh& mesh, const SceneFrame& frame, const Footprint& fp,
                      const glm::vec3& color) {
    std::vector<std::vector<glm::vec2>> rings;
    rings.push_back(localRing(frame, fp.outer, true));
    if (rings[0].empty()) return false;

    // Skip buildings entirely outside the scene.
    glm::vec2 lo = rings[0][0], hi = rings[0][0];
    for (size_t i = 1; i < rings[0].size(); i++) {
        lo = glm::min(lo, rings[0][i]);
        hi = glm::max(hi, rings[0][i]);
    }
    if (hi.x < frame.minLocal.x || lo.x > frame.maxLocal.x || hi.y < frame.minLocal.y || lo.y > frame.maxLocal.y) {
        return false;
    }

    for (size_t h = 0; h < fp.holes.size(); h++) {
        std::vector<glm::vec2> hole = localRing(frame, fp.holes[h], false);
        if (!hole.empty()) rings.push_back(hole);
    }

    const std::vector<uint32_t> indices = triangulate(rings);
    if (indices.empty()) return false;

    // Stand on the lowest ground under the outline so it never floats.
    float base = std::numeric_limits<float>::max();
    for (size_t i = 0; i < rings[0].size(); i++) {
        base = std::min(base, frame.groundHeight(rings[0][i].x, rings[0][i].y));
    }
    const float top = base + fp.height;

    // Roof: indices refer to the rings' points taken in order.
    std::vector<glm::vec3> roof;
    for (size_t r = 0; r < rings.size(); r++) {
        for (size_t i = 0; i < rings[r].size(); i++) roof.push_back(glm::vec3(rings[r][i], top));
    }
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        mesh.addTriangle(roof[indices[i]], roof[indices[i + 1]], roof[indices[i + 2]], up, color);
    }

    // Walls. Outlines run counter-clockwise and holes clockwise, so the
    // right-hand side of each edge is always outside the building.
    for (size_t r = 0; r < rings.size(); r++) {
        const std::vector<glm::vec2>& ring = rings[r];
        for (size_t i = 0; i < ring.size(); i++) {
            const glm::vec2 a = ring[i], b = ring[(i + 1) % ring.size()];
            const glm::vec2 d = b - a;
            const glm::vec3 normal = glm::normalize(glm::vec3(d.y, -d.x, 0.0f));
            mesh.addQuad(glm::vec3(a, base), glm::vec3(b, base), glm::vec3(b, top), glm::vec3(a, top), normal, color);
        }
    }
    return true;
}

// --- BuildingsLayer ----------------------------------------------------------

void BuildingsLayer::load(const SceneFrame& frame) {
    mMesh.clear();
    if (!mTried || mTriedFor != frame.latLon) {
        mTried = true;
        mTriedFor = frame.latLon;
        mDataOk = mData.load(frame.latLon, mDataNote);
    }
    note = mDataNote;
    if (!mDataOk) return;

    const std::vector<Footprint>& fps = mData.footprints();
    for (size_t i = 0; i < fps.size(); i++) {
        extrudeFootprint(mMesh, frame, fps[i], buildingColor(fps[i].id + long(i)));
    }
    mMesh.upload(GL_TRIANGLES);
}

void BuildingsLayer::draw(const glm::mat4& viewProj) {
    mShader.use(viewProj);
    mMesh.draw();
}
