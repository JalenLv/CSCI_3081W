#include "Elevation.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include "Cache.h"
#include "Config.h"
#include "Fetchers.h"

namespace {
std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = char(std::tolower((unsigned char)s[i]));
    return s;
}

// First few characters of a file, for error messages.
std::string preview(const std::string& text) {
    std::string s = text.substr(0, 40);
    for (size_t i = 0; i < s.size(); i++) {
        if (!std::isprint((unsigned char)s[i])) s[i] = ' ';
    }
    return s;
}

// Plausible heights in meters. Anything outside is treated as missing, which
// also catches fill values like -3.4e38 in files without a NODATA_value.
bool plausibleHeight(double v) {
    return std::isfinite(v) && v > -12000.0 && v < 10000.0;
}
}

void ElevationGrid::clear() {
    mHeights.clear();
    mValid.clear();
    mCols = mRows = 0;
    mCellLon = mCellLat = 0.0;
    mBounds = geo::LatLonBox();
    mMin = mMax = 0.0f;
    mLabel.clear();
    mDataset.clear();
    mLoaded = false;
}

bool ElevationGrid::load(const geo::LatLonBox& scene, std::string& note) {
    clear();
    note.clear();

    std::string error, dataset;
    const datafiles::Match match = datafiles::find(config::kTerrainDir, "dem", ".asc", scene);
    if (!match.path.empty()) {
        if (!parse(tiles::readFile(match.path), error)) {
            clear();
            note = "Can't read " + match.path + ": " + error;
            return false;
        }
        dataset = match.dataset;
        if (!match.full) note = "Terrain file covers only part of the scene.";
    } else {
        // Nothing on disk overlaps the scene: ask the fetcher for a bit more
        // than the scene, then cache the response like an imagery tile.
        const geo::LatLonBox request = scene.paddedAndRounded(config::kFetchPadding);
        std::string fetchError;
        const std::string body = fetchDem(request, fetchError);
        if (body.empty()) {
            note = "No terrain for this scene (" + fetchError + "). Download " + request.describe() +
                   " - see README.md.";
            return false;
        }
        if (!parse(body, error)) {
            clear();
            note = "Downloaded terrain isn't a valid grid: " + error;
            return false;
        }
        dataset = config::kDemDataset;
        tiles::writeFile(datafiles::pathFor(config::kTerrainDir, "dem", dataset, request, ".asc"), body);
    }

    computeRange(scene);

    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s, %.0f m cells", dataset.empty() ? "terrain" : dataset.c_str(),
                  mCellLat * 111320.0);
    mLabel = buf;
    mDataset = dataset;
    mLoaded = true;
    return true;
}

// Arc ASCII grid: a header of "key value" lines, then nrows x ncols values,
// north row first. Keys are case-insensitive. Accepts cellsize or dx/dy,
// xllcorner/yllcorner or xllcenter/yllcenter, and an optional NODATA_value.
bool ElevationGrid::parse(const std::string& text, std::string& error) {
    clear();
    if (text.empty()) {
        error = "the file is empty";
        return false;
    }
    if (text.size() >= 2 && text[0] == 'P' && text[1] == 'K') {
        error = "the file is a zip; unzip it and use the .asc inside";
        return false;
    }
    const std::string notAGrid = "not an Arc ASCII grid (starts with \"" + preview(text) + "\")";

    // c_str() is NUL-terminated, so strtod can't run past the end.
    const char* p = text.c_str();
    const char* end = p + text.size();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    double ncols = -1, nrows = -1, xll = nan, yll = nan, cell = -1, dx = -1, dy = -1, nodata = nan;
    bool xCenter = false, yCenter = false;
    int keys = 0;

    for (;;) {
        while (p < end && std::isspace((unsigned char)*p)) p++;
        if (p >= end || !std::isalpha((unsigned char)*p)) break;  // data starts with a number
        const char* k = p;
        while (p < end && !std::isspace((unsigned char)*p)) p++;
        const std::string key = lower(std::string(k, p));
        char* q = nullptr;
        const double v = std::strtod(p, &q);
        if (q == p) {
            error = notAGrid;
            return false;
        }
        p = q;
        keys++;

        if (key == "ncols") ncols = v;
        else if (key == "nrows") nrows = v;
        else if (key == "xllcorner") { xll = v; xCenter = false; }
        else if (key == "xllcenter") { xll = v; xCenter = true; }
        else if (key == "yllcorner") { yll = v; yCenter = false; }
        else if (key == "yllcenter") { yll = v; yCenter = true; }
        else if (key == "cellsize") cell = v;
        else if (key == "dx") dx = v;
        else if (key == "dy") dy = v;
        else if (key == "nodata_value") nodata = v;
        else {
            error = notAGrid;
            return false;
        }
    }

    if (keys == 0) {
        error = notAGrid;
        return false;
    }
    if (ncols < 1 || nrows < 1 || !std::isfinite(xll) || !std::isfinite(yll)) {
        error = "the header is missing ncols, nrows, xllcorner or yllcorner";
        return false;
    }
    const double cellX = cell > 0 ? cell : dx;
    const double cellY = cell > 0 ? cell : dy;
    if (!(cellX > 0) || !(cellY > 0)) {
        error = "the header is missing cellsize (or dx and dy)";
        return false;
    }
    if (xCenter) xll -= cellX / 2.0;
    if (yCenter) yll -= cellY / 2.0;

    const int cols = int(ncols), rows = int(nrows);
    const size_t total = size_t(cols) * size_t(rows);
    if (total > size_t(100) * 1000 * 1000) {
        error = "the grid is too large";
        return false;
    }
    const geo::LatLonBox bounds(yll, xll, yll + rows * cellY, xll + cols * cellX);
    if (bounds.west < -181.0 || bounds.east > 181.0 || bounds.south < -91.0 || bounds.north > 91.0) {
        error = "the grid isn't in degrees of lat/lon (projected coordinates aren't supported)";
        return false;
    }

    mHeights.assign(total, 0.0f);
    mValid.assign(total, 0);
    size_t count = 0, validCount = 0;
    while (count < total) {
        char* q = nullptr;
        const double v = std::strtod(p, &q);
        if (q == p) break;
        p = q;
        const bool ok = plausibleHeight(v) && !(v == nodata);
        if (ok) {
            mHeights[count] = float(v);
            mValid[count] = 1;
            validCount++;
        }
        count++;
    }
    if (count < total) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "the grid is cut short (%zu of %zu values)", count, total);
        error = buf;
        clear();
        return false;
    }
    if (validCount == 0) {
        error = "the grid has no valid heights (all NODATA)";
        clear();
        return false;
    }

    mCols = cols;
    mRows = rows;
    mCellLon = cellX;
    mCellLat = cellY;
    mBounds = bounds;
    fillNoData();
    return true;
}

// Gives every NODATA cell the height of the nearest valid cell (breadth-first
// from all valid cells at once, so it's linear in the grid size).
void ElevationGrid::fillNoData() {
    std::vector<uint8_t> done(mValid);
    std::vector<size_t> queue;
    queue.reserve(mHeights.size());
    for (size_t i = 0; i < mValid.size(); i++) {
        if (mValid[i]) queue.push_back(i);
    }
    if (queue.size() == mHeights.size()) return;

    for (size_t head = 0; head < queue.size(); head++) {
        const size_t i = queue[head];
        const int c = int(i % size_t(mCols)), r = int(i / size_t(mCols));
        const int nc[4] = { c - 1, c + 1, c, c };
        const int nr[4] = { r, r, r - 1, r + 1 };
        for (int k = 0; k < 4; k++) {
            if (nc[k] < 0 || nr[k] < 0 || nc[k] >= mCols || nr[k] >= mRows) continue;
            const size_t n = size_t(nr[k]) * mCols + nc[k];
            if (done[n]) continue;
            done[n] = 1;
            mHeights[n] = mHeights[i];
            queue.push_back(n);
        }
    }
}

// Lowest and highest valid heights among the cells whose centers are inside
// the scene (the whole grid if none are).
void ElevationGrid::computeRange(const geo::LatLonBox& scene) {
    float lo = std::numeric_limits<float>::max(), hi = -std::numeric_limits<float>::max();
    bool any = false;
    for (int pass = 0; pass < 2 && !any; pass++) {
        for (int r = 0; r < mRows; r++) {
            const double lat = mBounds.north - (r + 0.5) * mCellLat;
            if (pass == 0 && (lat < scene.south || lat > scene.north)) continue;
            for (int c = 0; c < mCols; c++) {
                const double lon = mBounds.west + (c + 0.5) * mCellLon;
                if (pass == 0 && (lon < scene.west || lon > scene.east)) continue;
                const size_t i = size_t(r) * mCols + c;
                if (!mValid[i]) continue;
                lo = std::min(lo, mHeights[i]);
                hi = std::max(hi, mHeights[i]);
                any = true;
            }
        }
    }
    mMin = any ? lo : 0.0f;
    mMax = any ? hi : 0.0f;
}

float ElevationGrid::sampleLatLon(double lat, double lon) const {
    if (!mLoaded) return 0.0f;
    // Cell centers sit half a cell in from the edges.
    double fx = (lon - mBounds.west) / mCellLon - 0.5;
    double fy = (mBounds.north - lat) / mCellLat - 0.5;
    fx = std::min(std::max(fx, 0.0), double(mCols - 1));
    fy = std::min(std::max(fy, 0.0), double(mRows - 1));
    const int x0 = int(fx), y0 = int(fy);
    const int x1 = std::min(x0 + 1, mCols - 1), y1 = std::min(y0 + 1, mRows - 1);
    const float tx = float(fx - x0), ty = float(fy - y0);
    const float a = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
    const float b = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
    return a + (b - a) * ty;
}

float ElevationGrid::sample(double mx, double my) const {
    return sampleLatLon(geo::mercYToLat(my), geo::mercXToLon(mx));
}
