#ifndef MAPVIEWER_CACHE_H_
#define MAPVIEWER_CACHE_H_

// Files on disk: the imagery tile cache (tiles::) and the terrain/building
// data files picked by area (datafiles::).

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

#include "Config.h"
#include "Geo.h"
#include "WebService.h"

// ---------------------------------------------------------------------------
// Tile downloading and caching
//
// Tiles are cached as the raw bytes the server sent (no re-encoding).
// ---------------------------------------------------------------------------

namespace tiles {
struct Job {
    int x, y, z;
    std::string route;      // path appended to the server URL
    std::string cacheFile;
    std::string data;       // image bytes once fetched
};

inline bool fileExists(const std::string& name) {
    struct stat buffer;
    return stat(name.c_str(), &buffer) == 0;
}

inline std::string readFile(const std::string& path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline void writeFile(const std::string& path, const std::string& data) {
    std::ofstream out(path.c_str(), std::ios::binary);
    out.write(data.data(), std::streamsize(data.size()));
}

// WebService returns an empty string (or an error page) when a request
// fails, so check for JPEG/PNG bytes before trusting the data.
inline bool looksLikeImage(const std::string& d) {
    if (d.size() < 4) return false;
    const unsigned char* b = (const unsigned char*)d.data();
    bool jpeg = b[0] == 0xFF && b[1] == 0xD8;
    bool png = b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G';
    return jpeg || png;
}

// Fills every job's data: from the cache when present, otherwise downloaded
// (several at a time) and cached if it's a real image. Worker threads only
// fetch bytes; decoding and GL work stay on the main thread.
// Returns how many tiles were requested from the server.
inline int fetchAll(std::vector<Job>& jobs, const char* server) {
    std::vector<Job*> todo;
    for (size_t i = 0; i < jobs.size(); i++) {
        if (fileExists(jobs[i].cacheFile)) jobs[i].data = readFile(jobs[i].cacheFile);
        else todo.push_back(&jobs[i]);
    }
    if (todo.empty()) return 0;

    std::atomic<size_t> next(0);
    auto worker = [&todo, &next, server]() {
        gis::WebService ws(server);
        for (size_t i = next++; i < todo.size(); i = next++) {
            todo[i]->data = ws.get(todo[i]->route);
        }
    };

    size_t threadCount = std::min<size_t>(config::kDownloadThreads, todo.size());
    std::vector<std::thread> threads;
    for (size_t i = 0; i < threadCount; i++) threads.emplace_back(worker);
    for (size_t i = 0; i < threads.size(); i++) threads[i].join();

    for (size_t i = 0; i < todo.size(); i++) {
        if (looksLikeImage(todo[i]->data)) writeFile(todo[i]->cacheFile, todo[i]->data);
    }
    return (int)todo.size();
}
}

// ---------------------------------------------------------------------------
// Data files named by the area they cover (see README.md, 6.1):
//   <prefix>_[<dataset>_]<S>_<W>_<N>_<E><ext>
//   e.g. dem_USGS10m_44.939_-93.288_45.009_-93.168.asc
//        buildings_44.939_-93.288_45.009_-93.168.json
// ---------------------------------------------------------------------------

namespace datafiles {
struct Match {
    std::string path;       // empty when no file overlaps the scene
    std::string dataset;    // e.g. "USGS10m"; empty if the name has none
    geo::LatLonBox box;     // area from the file name
    bool full = false;      // true if the box covers the whole scene
};

inline bool parseNumber(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end == s.c_str() + s.size();
}

// Parses a file name (no directory). Returns false if it doesn't match.
inline bool parseName(const std::string& name, const std::string& prefix, const std::string& ext,
                      geo::LatLonBox& box, std::string& dataset) {
    const std::string head = prefix + "_";
    if (name.size() <= head.size() + ext.size()) return false;
    if (name.compare(0, head.size(), head) != 0) return false;
    if (name.compare(name.size() - ext.size(), ext.size(), ext) != 0) return false;

    std::vector<std::string> parts;
    std::stringstream ss(name.substr(head.size(), name.size() - head.size() - ext.size()));
    std::string part;
    while (std::getline(ss, part, '_')) parts.push_back(part);
    if (parts.size() < 4) return false;

    const size_t n = parts.size();
    double v[4];
    for (int i = 0; i < 4; i++) {
        if (!parseNumber(parts[n - 4 + i], v[i])) return false;
    }
    box = geo::LatLonBox(v[0], v[1], v[2], v[3]);
    if (!box.valid()) return false;

    dataset.clear();
    for (size_t i = 0; i + 4 < n; i++) dataset += (i ? "_" : "") + parts[i];
    return true;
}

// Picks the file for `scene` in `dir`: the smallest one that covers the whole
// scene, else the one that overlaps it most, else none.
inline Match find(const std::string& dir, const std::string& prefix, const std::string& ext,
                  const geo::LatLonBox& scene) {
    std::vector<std::string> names;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) names.push_back(e->d_name);
        closedir(d);
    }
    std::sort(names.begin(), names.end());  // readdir order is arbitrary

    Match best;
    double bestScore = 0.0;
    for (size_t i = 0; i < names.size(); i++) {
        geo::LatLonBox box;
        std::string dataset;
        if (!parseName(names[i], prefix, ext, box, dataset)) continue;

        const bool full = box.contains(scene);
        const double overlap = box.intersect(scene).area();
        if (overlap <= 0.0) continue;

        bool better;
        if (full != best.full) better = full;                  // full coverage always wins
        else if (full) better = best.path.empty() || box.area() < bestScore;  // smallest
        else better = overlap > bestScore;                       // most overlap
        if (!better) continue;

        best.path = dir + "/" + names[i];
        best.dataset = dataset;
        best.box = box;
        best.full = full;
        bestScore = full ? box.area() : overlap;
    }
    return best;
}

// Path a fetched file is saved under. `dataset` may be empty.
inline std::string pathFor(const std::string& dir, const std::string& prefix, const std::string& dataset,
                           const geo::LatLonBox& box, const std::string& ext) {
    return dir + "/" + prefix + "_" + (dataset.empty() ? "" : dataset + "_") + box.fileToken() + ext;
}
}

#endif
