#include "MapViewer.h"

// Graphics.h brings in glad, which must come before any NanoGUI/GLFW header.
#include "Graphics.h"

#include <nanogui/opengl.h>
#include <nanogui/screen.h>
#include <nanogui/window.h>
#include <nanogui/layout.h>
#include <nanogui/label.h>
#include <nanogui/button.h>
#include <nanogui/checkbox.h>
#include <nanogui/textbox.h>
#include <nanogui/slider.h>
#include <nanovg.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <curl/curl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "Buildings.h"
#include "Cache.h"
#include "Config.h"
#include "Elevation.h"
#include "Geo.h"
#include "Routes.h"
#include "Scene.h"

namespace {

// ---------------------------------------------------------------------------
// Orbit camera for the 3D view (all values in local meters / degrees)
// ---------------------------------------------------------------------------

struct OrbitCamera {
    glm::vec3 target = glm::vec3(0.0f);
    float distance = 1000.0f;
    float heading = 0.0f;  // clockwise from north; 0 = north up
    float pitch = 0.0f;    // from straight down; 0 = top-down

    glm::vec3 eye() const {
        float h = glm::radians(heading), p = glm::radians(pitch);
        return target + distance * glm::vec3(-std::sin(p) * std::sin(h), -std::sin(p) * std::cos(h), std::cos(p));
    }

    glm::mat4 view() const {
        float h = glm::radians(heading), p = glm::radians(pitch);
        // Screen-up direction; stays well defined when looking straight down.
        glm::vec3 up(std::sin(h) * std::cos(p), std::cos(h) * std::cos(p), std::sin(p));
        return glm::lookAt(eye(), target, up);
    }

    glm::mat4 projection(float aspect, float sceneSize) const {
        float nearPlane = std::max(distance * 0.01f, 0.1f);
        float farPlane = distance * 4.0f + sceneSize * 3.0f;
        return glm::perspective(glm::radians(config::kFovY), aspect, nearPlane, farPlane);
    }

    // Ground meters per screen pixel at the target.
    float metersPerPixel(float viewportHeight) const {
        return 2.0f * distance * std::tan(glm::radians(config::kFovY) / 2.0f) / viewportHeight;
    }

    // Ground directions that match screen right / screen up.
    glm::vec3 right() const {
        float h = glm::radians(heading);
        return glm::vec3(std::cos(h), -std::sin(h), 0.0f);
    }
    glm::vec3 forward() const {
        float h = glm::radians(heading);
        return glm::vec3(std::sin(h), std::cos(h), 0.0f);
    }
};

// Adds `line` to a newline-separated list of notes.
void appendNote(std::string& notes, const std::string& line) {
    if (line.empty()) return;
    if (!notes.empty()) notes += "\n";
    notes += line;
}

// Splits text into lines no wider than maxWidth with the current NanoVG
// font.
std::vector<std::string> wrapText(NVGcontext* ctx, const std::string& text, float maxWidth) {
    std::vector<std::string> out;
    std::istringstream paragraphs(text);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph, '\n')) {
        std::istringstream words(paragraph);
        std::string word, line;
        while (words >> word) {
            const std::string trial = line.empty() ? word : line + " " + word;
            if (!line.empty() && nvgTextBounds(ctx, 0.0f, 0.0f, trial.c_str(), nullptr, nullptr) > maxWidth) {
                out.push_back(line);
                line = word;
            } else {
                line = trial;
            }
        }
        out.push_back(line);
    }
    if (out.empty()) out.push_back("");
    return out;
}

// ---------------------------------------------------------------------------
// The window: GUI, input, 2D map, 3D scene and HUD
// ---------------------------------------------------------------------------

class MapViewerScreen : public nanogui::Screen {
public:
    MapViewerScreen()
        : nanogui::Screen(Eigen::Vector2i(1100, 800), "GopherMap Viewer",
                          /*resizable*/ true, /*fullscreen*/ false,
                          /*colorBits*/ 8, /*alphaBits*/ 8, /*depthBits*/ 24,
                          /*stencilBits*/ 8, /*nSamples*/ 0,
                          config::kGlMajor, config::kGlMinor) {
        // NanoGUI calls GL directly on Linux, and everything here goes through glad.
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            throw std::runtime_error("Failed to initialize GLAD");
        }

        mTerrainShader.init();
        mColorShader.init();
        mUnitQuad.init();

        mLayers.push_back(std::unique_ptr<MapLayer>(new RoutesLayer(mColorShader)));
        mLayers.push_back(std::unique_ptr<MapLayer>(new BuildingsLayer(mColorShader)));

        mZoom = config::kStartZoom;
        setCenter({ config::kStartLat, config::kStartLon });
        mPendingLocation = centerLatLon();

        buildGui();
        updateControls();
        setStatus("Press Render to load imagery.");
    }

    ~MapViewerScreen() {
        // GL context is still alive here.
        unloadLayers();
        mTiles.clear();
        mUnitQuad.destroy();
        mColorShader.destroy();
        mTerrainShader.destroy();
    }

    // --- Rendering --------------------------------------------------------

    void drawContents() override {
        // Slow work (downloads) waits until its notice has been on screen for
        // one frame, so the window doesn't look frozen.
        if (mPendingTask && mNoticeDrawn) {
            std::function<void()> task = mPendingTask;
            mPendingTask = nullptr;
            task();
            updateControls();
        }

        updateAnimation();

        glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // NanoVG drew the GUI last frame and may have left state behind.
        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);

        if (mMode == Mode::Map2D) draw2D();
        else draw3D();

        glDisable(GL_DEPTH_TEST);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }

    void draw(NVGcontext* ctx) override {
        drawMapOverlay(ctx);  // under the panel
        Screen::draw(ctx);    // the panel
        drawHud(ctx);         // on top
        if (mPendingTask) mNoticeDrawn = true;
    }

    // --- Input ------

    bool keyboardEvent(int key, int scancode, int action, int modifiers) override {
        bool wasEditing = mCoordBox->focused();
        if (Screen::keyboardEvent(key, scancode, action, modifiers)) {
            // Enter in the location box.
            if (wasEditing && key == GLFW_KEY_ENTER && action == GLFW_PRESS)
                goToPending();
            return true;
        }
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            if (mMode == Mode::Scene3D) {
                if (!busy()) startAnimation(true);
            } else {
                setVisible(false);
            }
            return true;
        }
        return false;
    }

    bool scrollEvent(const Eigen::Vector2i& p, const Eigen::Vector2f& rel) override {
        if (Screen::scrollEvent(p, rel))
            return true;  // scrolled over the panel
        if (mDragMode != DragMode::None || mAnim.active)
            return true;

        if (mMode == Mode::Scene3D) {
            float dist = mCam.distance * std::pow(config::kScrollDollyFactor, rel.y());
            mCam.distance = clampDistance(dist);
            return true;
        }

        // 2D: zoom around the cursor keeping the map point under it fixed.
        glm::dvec2 anchor = screenToMerc(p);
        mZoom = clampZoom(mZoom + rel.y() * config::kScrollZoomStep);
        double wp = worldPx();
        mCenter.x = anchor.x - (p.x() - size().x() / 2.0) / wp;
        mCenter.y = anchor.y - (p.y() - size().y() / 2.0) / wp;
        clampCenter();
        updateControls();
        return true;
    }

    bool mouseButtonEvent(const Eigen::Vector2i& p, int button, bool down, int modifiers) override {
        // Finish a drag even if the button is released over the panel.
        if (mDragMode != DragMode::None && !down && button == mDragButton) {
            mDragMode = DragMode::None;
            return true;
        }

        if (Screen::mouseButtonEvent(p, button, down, modifiers))
            return true;  // clicked a widget
        if (!down || mDragMode != DragMode::None || mAnim.active)
            return false;

        bool shift = (modifiers & GLFW_MOD_SHIFT) != 0;
        if (mMode == Mode::Map2D) {
            if (button != GLFW_MOUSE_BUTTON_LEFT) return false;
            mDragMode = DragMode::Pan2D;
        } else {
            // Blender-ish: left or middle drag orbits, Shift+drag or right drag pans.
            if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE)
                mDragMode = shift ? DragMode::Pan3D : DragMode::Orbit;
            else if (button == GLFW_MOUSE_BUTTON_RIGHT)
                mDragMode = DragMode::Pan3D;
            else
                return false;
        }

        mDragButton = button;
        mDragStartPx = p;
        mLastDragPx = p;
        mDragStartCenter = mCenter;
        mDragStartHeading = mCam.heading;
        mDragStartPitch = mCam.pitch;
        return true;
    }

    bool mouseMotionEvent(const Eigen::Vector2i& p, const Eigen::Vector2i& rel, int button, int modifiers) override {
        mCursorPx = p;
        mCursorValid = true;

        switch (mDragMode) {
        case DragMode::Pan2D: {
            double wp = worldPx();
            mCenter.x = mDragStartCenter.x - (p.x() - mDragStartPx.x()) / wp;
            mCenter.y = mDragStartCenter.y - (p.y() - mDragStartPx.y()) / wp;
            clampCenter();
            return true;
        }
        case DragMode::Orbit: {
            // Turntable: horizontal drag spins, dragging up tilts the far side away.
            float heading = mDragStartHeading + (p.x() - mDragStartPx.x()) * config::kOrbitDegPerPixel;
            mCam.heading = std::fmod(std::fmod(heading, 360.0f) + 360.0f, 360.0f);
            mCam.pitch = glm::clamp(mDragStartPitch - (p.y() - mDragStartPx.y()) * config::kOrbitDegPerPixel,
                                    0.0f, config::kMaxPitch);
            return true;
        }
        case DragMode::Pan3D: {
            // The ground follows the cursor.
            float mpp = mCam.metersPerPixel(float(size().y()));
            float dx = float(p.x() - mLastDragPx.x());
            float dy = float(p.y() - mLastDragPx.y());
            mCam.target += -mCam.right() * dx * mpp + mCam.forward() * dy * mpp;
            clampTarget();
            mLastDragPx = p;
            return true;
        }
        case DragMode::None:
            break;
        }
        return Screen::mouseMotionEvent(p, rel, button, modifiers);
    }

private:
    enum class Mode { Map2D, Scene3D };
    enum class DragMode { None, Pan2D, Orbit, Pan3D };

    struct CameraAnimation {
        bool active = false;
        bool exitAfter = false;  // switch to 2D when finished
        double startTime = 0.0;
        float startHeading = 0.0f;
        float startPitch = 0.0f;
    };

    // --- GUI ----------------------------------------------------------------

    nanogui::Widget* makeRow(nanogui::Widget* parent) {
        nanogui::Widget* row = new nanogui::Widget(parent);
        row->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal,
                                              nanogui::Alignment::Middle, 0, 6));
        return row;
    }

    void buildGui() {
        nanogui::Window* panel = new nanogui::Window(this, "Map Controls");
        panel->setPosition(Eigen::Vector2i(15, 15));
        panel->setLayout(new nanogui::GroupLayout());

        // Location box + Go
        new nanogui::Label(panel, "Location (lat, lon)", "sans-bold");
        nanogui::Widget* locRow = makeRow(panel);
        mCoordBox = new nanogui::TextBox(locRow, geo::format(centerLatLon()));
        mCoordBox->setEditable(true);
        mCoordBox->setFixedSize(Eigen::Vector2i(180, 26));
        mCoordBox->setAlignment(nanogui::TextBox::Alignment::Left);
        mCoordBox->setTooltip("e.g. 44.97400, -93.22800 - press Enter or Go");
        // Called when editing ends (Enter or clicking away). Returning false
        // makes the box revert to its previous text.
        mCoordBox->setCallback([this](const std::string& text) {
            geo::LatLon p;
            mPendingValid = geo::parse(text, p);
            if (mPendingValid) {
                mPendingLocation = p;
            } else {
                setError("Invalid location. Use \"lat, lon\".");
            }
            return mPendingValid;
        });
        mGoButton = new nanogui::Button(locRow, "Go");
        mGoButton->setFixedWidth(50);
        mGoButton->setCallback([this]() { goToPending(); });

        // View
        new nanogui::Label(panel, "View", "sans-bold");
        nanogui::Widget* zoomRow = makeRow(panel);
        mZoomInButton = new nanogui::Button(zoomRow, "Zoom in");
        mZoomInButton->setFixedWidth(115);
        mZoomInButton->setCallback([this]() { stepZoom(+1); });
        mZoomOutButton = new nanogui::Button(zoomRow, "Zoom out");
        mZoomOutButton->setFixedWidth(115);
        mZoomOutButton->setCallback([this]() { stepZoom(-1); });

        mRenderButton = new nanogui::Button(panel, "Render");
        mRenderButton->setBackgroundColor(nanogui::Color(0, 110, 200, 255));
        mRenderButton->setCallback([this]() {
            runAfterNotice("Rendering...", [this]() { renderScene(); });
        });

        mModeButton = new nanogui::Button(panel, "3D view");
        mModeButton->setCallback([this]() {
            if (mMode == Mode::Scene3D) {
                startAnimation(true);
            } else if (mHaveRender && renderMatchesView()) {
                runAfterNotice("Opening 3D view...", [this]() { enter3D(); });
            } else {
                runAfterNotice("Rendering, then opening 3D view...", [this]() {
                    if (renderScene()) enter3D();
                });
            }
        });

        mTopViewButton = new nanogui::Button(panel, "Top view");
        mTopViewButton->setTooltip("Return the 3D camera to top-down, north up");
        mTopViewButton->setCallback([this]() { startAnimation(false); });

        // Terrain
        new nanogui::Label(panel, "Terrain", "sans-bold");
        mElevationBox = new nanogui::CheckBox(panel, "Elevation", [this](bool on) {
            mUseElevation = on;
            if (mMode == Mode::Scene3D) applyTerrainSettings();
            updateControls();
        });
        mElevationBox->setChecked(mUseElevation);

        nanogui::Widget* exagRow = makeRow(panel);
        new nanogui::Label(exagRow, "Exaggeration");
        mExaggerationSlider = new nanogui::Slider(exagRow);
        mExaggerationSlider->setRange(std::make_pair(1.0f, config::kMaxExaggeration));
        mExaggerationSlider->setValue(mExaggeration);
        mExaggerationSlider->setFixedWidth(100);
        mExaggerationLabel = new nanogui::Label(exagRow, exaggerationText(mExaggeration));
        mExaggerationLabel->setFixedWidth(40);
        // Update the label while dragging; rebuild the terrain on release.
        mExaggerationSlider->setCallback([this](float v) {
            mExaggerationLabel->setCaption(exaggerationText(v));
        });
        mExaggerationSlider->setFinalCallback([this](float v) {
            mExaggeration = v;
            if (mMode == Mode::Scene3D && mUseElevation) applyTerrainSettings();
        });

        // Layers: in 2D, choose what 3D view shows; in 3D, show/hide live.
        new nanogui::Label(panel, "Layers (min zoom " + std::to_string(config::kMinFeatureZoom) + ")", "sans-bold");
        for (size_t i = 0; i < mLayers.size(); i++) {
            nanogui::CheckBox* box = new nanogui::CheckBox(panel, mLayers[i]->name(), [this, i](bool on) {
                mLayers[i]->visible = on;
                if (on && mMode == Mode::Scene3D && layersAllowed() && !mLayers[i]->loaded) {
                    // Loading can read a large file, so show a notice first.
                    runAfterNotice("Loading " + mLayers[i]->name() + "...", [this, i]() {
                        loadLayer(*mLayers[i]);
                        if (mLayers[i]->note.empty()) setStatus(mLayers[i]->name() + " loaded.");
                        else setError(mLayers[i]->note);
                    });
                }
            });
            box->setChecked(mLayers[i]->visible);
            mLayerBoxes.push_back(box);
        }

        performLayout();
    }

    static std::string exaggerationText(float v) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "x%.1f", v);
        return buf;
    }

    bool busy() const { return bool(mPendingTask) || mAnim.active; }

    // Enables/disables widgets for the current mode.
    void updateControls() {
        const bool idle = !busy();
        const bool map = mMode == Mode::Map2D && idle;

        mCoordBox->setEditable(map);
        mCoordBox->setEnabled(map);
        mGoButton->setEnabled(map);
        mZoomInButton->setEnabled(map);
        mZoomOutButton->setEnabled(map);
        mRenderButton->setEnabled(map);

        mModeButton->setCaption(mMode == Mode::Map2D ? "3D view" : "Back to 2D map");
        mModeButton->setEnabled(idle);
        mTopViewButton->setEnabled(mMode == Mode::Scene3D && idle);

        mElevationBox->setEnabled(idle);
        mExaggerationSlider->setEnabled(idle && mUseElevation);

        // Layers need enough detail: the zoom Render would use (2D) or the
        // zoom the scene was frozen at (3D).
        bool layersOk = mMode == Mode::Map2D ? tileZoom() >= config::kMinFeatureZoom : layersAllowed();
        for (size_t i = 0; i < mLayerBoxes.size(); i++) mLayerBoxes[i]->setEnabled(layersOk && idle);
    }

    void setStatus(const std::string& s) { mStatus = s; mStatusIsError = false; }
    void setError(const std::string& s) {
        mStatus = s;
        mStatusIsError = true;
        std::cerr << "[MapViewer] " << s << std::endl;  // the full text, in case the HUD wraps it
    }

    // Shows `notice`, waits one frame so it's visible, then runs `task`.
    void runAfterNotice(const std::string& notice, std::function<void()> task) {
        if (mPendingTask) return;
        mPendingTask = task;
        mNoticeDrawn = false;
        setStatus(notice);
        updateControls();
    }

    // --- 2D camera ------------------------------------------------------------

    static double clampZoom(double z) {
        return std::min(std::max(z, config::kMinZoom), config::kMaxZoom);
    }

    // Whole zoom level Render will load tiles from.
    int tileZoom() const {
        return (int)clampZoom(std::floor(mZoom + 0.5));
    }

    // Size of the whole world in screen pixels at the current zoom.
    double worldPx() const {
        return config::kTileSize * std::pow(2.0, mZoom);
    }

    void clampCenter() {
        mCenter.x = std::min(std::max(mCenter.x, 0.0), 1.0);
        mCenter.y = std::min(std::max(mCenter.y, 0.0), 1.0);
    }

    void setCenter(const geo::LatLon& p) {
        mCenter = glm::dvec2(geo::lonToMercX(p.lon), geo::latToMercY(p.lat));
        clampCenter();
    }

    geo::LatLon centerLatLon() const {
        return { geo::mercYToLat(mCenter.y), geo::mercXToLon(mCenter.x) };
    }

    glm::dvec2 screenToMerc(const Eigen::Vector2i& p) const {
        double wp = worldPx();
        return glm::dvec2(mCenter.x + (p.x() - size().x() / 2.0) / wp,
                          mCenter.y + (p.y() - size().y() / 2.0) / wp);
    }

    glm::dvec2 mercToScreen(double mx, double my) const {
        double wp = worldPx();
        return glm::dvec2((mx - mCenter.x) * wp + size().x() / 2.0,
                          (my - mCenter.y) * wp + size().y() / 2.0);
    }

    geo::MercBounds visibleBounds() const {
        glm::dvec2 a = screenToMerc(Eigen::Vector2i(0, 0));
        glm::dvec2 b = screenToMerc(size());
        return { a.x, a.y, b.x, b.y };
    }

    // True ground meters per screen pixel at the 2D view center.
    double metersPerPixel() const {
        double lat = centerLatLon().lat * geo::kPi / 180.0;
        return geo::kEarthCircumference / config::kTileSize * std::cos(lat) / std::pow(2.0, mZoom);
    }

    // True when the last Render is at the zoom Render would use now and
    // covers everything currently on screen.
    bool renderMatchesView() const {
        if (!mHaveRender || tileZoom() != mRenderedZoom) return false;
        geo::MercBounds v = geo::clampToWorld(visibleBounds());
        const geo::MercBounds& c = mRenderedCoverage;
        const double eps = 1e-12;
        return v.minX >= c.minX - eps && v.maxX <= c.maxX + eps &&
               v.minY >= c.minY - eps && v.maxY <= c.maxY + eps;
    }

    // --- 2D actions -----------------------------------------------------------

    void stepZoom(int direction) {
        double z = direction > 0 ? std::floor(mZoom + 1e-9) + 1.0
                                 : std::ceil(mZoom - 1e-9) - 1.0;
        mZoom = clampZoom(z);
        updateControls();
    }

    void goToPending() {
        if (mMode != Mode::Map2D || busy()) return;
        if (!mPendingValid) return;  // last entry was invalid; error already shown
        setCenter(mPendingLocation);
        setStatus("Moved to " + geo::format(mPendingLocation) + ". Press Render.");
    }

    // Loads the imagery for the current view at tileZoom() and discards
    // everything that was rendered before. Returns true if any tiles loaded.
    bool renderScene() {
        const int z = tileZoom();
        const int n = 1 << z;
        const geo::MercBounds v = visibleBounds();
        const int x0 = geo::tileIndex(v.minX, n), x1 = geo::tileIndex(v.maxX, n);
        const int y0 = geo::tileIndex(v.minY, n), y1 = geo::tileIndex(v.maxY, n);

        const int count = (x1 - x0 + 1) * (y1 - y0 + 1);
        if (count > config::kMaxTilesPerRender) {
            setError("Too many tiles (" + std::to_string(count) + "). Shrink the window or zoom in.");
            return false;
        }

        std::vector<tiles::Job> jobs;
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                tiles::Job j;
                j.x = x;
                j.y = y;
                j.z = z;
                j.route = std::to_string(z) + "/" + std::to_string(y) + "/" + std::to_string(x);
                // World Imagery serves JPEG; the bytes are cached unchanged.
                j.cacheFile = config::kImageryCacheDir + "/" + std::to_string(z) + "-" +
                              std::to_string(y) + "-" + std::to_string(x) + ".jpg";
                jobs.push_back(j);
            }
        }

        const int downloaded = tiles::fetchAll(jobs, config::kImageryServer);

        mTiles.clear();
        int failed = 0;
        for (size_t i = 0; i < jobs.size(); i++) {
            std::unique_ptr<ImageryTile> tile(new ImageryTile(jobs[i].x, jobs[i].y, jobs[i].z));
            if (tile->setImage(jobs[i].data)) mTiles.push_back(std::move(tile));
            else failed++;
        }

        mHaveRender = !mTiles.empty();
        mRenderedZoom = z;
        mRenderedCoverage = { x0 / double(n), y0 / double(n),
                              (x1 + 1) / double(n), (y1 + 1) / double(n) };

        char buf[160];
        std::snprintf(buf, sizeof(buf), "Loaded %d tiles (%d downloaded, %d failed)",
                      (int)mTiles.size(), downloaded, failed);
        if (failed > 0) setError(buf);
        else setStatus(buf);
        return mHaveRender;
    }

    void draw2D() {
        glDisable(GL_DEPTH_TEST);
        mTerrainShader.begin(0.0f);  // no hillshading in 2D

        // Place each tile from its Mercator bounds, in double precision
        // relative to the camera; a float view matrix loses whole pixels of
        // accuracy past zoom ~17.
        const double wp = worldPx();
        const double W = size().x();
        const double H = size().y();

        for (size_t i = 0; i < mTiles.size(); i++) {
            const ImageryTile& t = *mTiles[i];
            const double n = double(1 << t.z);

            double left   =  2.0 * (t.x / n - mCenter.x) * wp / W;
            double right  =  2.0 * ((t.x + 1) / n - mCenter.x) * wp / W;
            double top    = -2.0 * (t.y / n - mCenter.y) * wp / H;
            double bottom = -2.0 * ((t.y + 1) / n - mCenter.y) * wp / H;

            if (right < -1.0 || left > 1.0 || top < -1.0 || bottom > 1.0)
                continue;  // off screen

            // Unit quad (0..1, v = 0 at the north edge) -> [left,right] x [top,bottom].
            glm::mat4 m(1.0f);
            m = glm::translate(m, glm::vec3(float(left), float(top), 0.0f));
            m = glm::scale(m, glm::vec3(float(right - left), float(bottom - top), 1.0f));

            mTerrainShader.setMVP(m);
            t.bindTexture();
            mUnitQuad.draw();
        }
    }

    // --- 3D view --------------------------------------------------------------

    bool layersAllowed() const { return mFrame.zoom >= config::kMinFeatureZoom; }

    void enter3D() {
        if (!mHaveRender) {
            setError("Render something first.");
            return;
        }

        mFrame.build(mRenderedCoverage, mRenderedZoom);
        mFrame.elevation = &mElevation;
        mFrame.useElevation = mUseElevation;
        mFrame.exaggeration = mExaggeration;

        std::string note;
        mElevation.clear();
        mElevationTried = false;
        if (mUseElevation) note = loadElevation();

        for (size_t i = 0; i < mTiles.size(); i++) mTiles[i]->buildMesh(mFrame, config::kTerrainGridPerTile);

        // Start exactly where the 2D view was: same center, same scale, top-down.
        mCam.target = mFrame.toLocal(centerLatLon());
        mCam.heading = 0.0f;
        mCam.pitch = 0.0f;
        float mpp = float(metersPerPixel());
        mCam.distance = clampDistance(mpp * size().y() / (2.0f * std::tan(glm::radians(config::kFovY) / 2.0f)));
        clampTarget();

        mMode = Mode::Scene3D;
        mDragMode = DragMode::None;

        if (layersAllowed()) {
            for (size_t i = 0; i < mLayers.size(); i++) {
                if (!mLayers[i]->visible) continue;
                loadLayer(*mLayers[i]);
                appendNote(note, mLayers[i]->note);
            }
        } else {
            appendNote(note, "Layers need zoom >= " + std::to_string(config::kMinFeatureZoom) + ".");
        }

        if (note.empty()) setStatus("3D view. Esc or Back to 2D map to leave.");
        else setError(note);
    }

    // Loads the terrain for the frozen area. Returns a problem note, or "".
    std::string loadElevation() {
        mElevationTried = true;
        std::string note;
        mElevation.load(mFrame.latLon, note);
        return note;
    }

    // Elevation on/off or exaggeration changed while in 3D: reshape the
    // terrain and rebuild the layers that sit on it. A failed terrain load
    // isn't retried until the next time the 3D view opens.
    void applyTerrainSettings() {
        mFrame.useElevation = mUseElevation;
        mFrame.exaggeration = mExaggeration;

        if (mUseElevation && !mElevationTried) {
            runAfterNotice("Loading elevation...", [this]() {
                std::string note = loadElevation();
                rebuildScene();
                if (note.empty()) setStatus("Elevation loaded.");
                else setError(note);
            });
            return;
        }
        rebuildScene();
    }

    void rebuildScene() {
        for (size_t i = 0; i < mTiles.size(); i++) mTiles[i]->buildMesh(mFrame, config::kTerrainGridPerTile);
        for (size_t i = 0; i < mLayers.size(); i++) {
            if (mLayers[i]->loaded) {
                mLayers[i]->unload();
                mLayers[i]->load(mFrame);  // layers keep their data; this only rebuilds geometry
            }
        }
        clampTarget();
    }

    void finishExit3D() {
        // Hand the 3D camera's position and scale back to the 2D map.
        geo::LatLon target = mFrame.toLatLon(mCam.target);
        setCenter(target);
        double mpp = mCam.metersPerPixel(float(size().y()));
        double lat = target.lat * geo::kPi / 180.0;
        mZoom = clampZoom(std::log2(geo::kEarthCircumference / config::kTileSize * std::cos(lat) / mpp));

        unloadLayers();
        for (size_t i = 0; i < mTiles.size(); i++) mTiles[i]->freeMesh();
        mElevation.clear();
        mElevationTried = false;
        mMode = Mode::Map2D;
        setStatus("Back on the 2D map. Move around and Render again.");
    }

    void loadLayer(MapLayer& layer) {
        if (layer.loaded) return;
        layer.load(mFrame);
        layer.loaded = true;
    }

    void unloadLayers() {
        for (size_t i = 0; i < mLayers.size(); i++) {
            if (mLayers[i]->loaded) mLayers[i]->unload();
            mLayers[i]->loaded = false;
        }
    }

    float clampDistance(float d) const {
        float lo = std::max(10.0f, mFrame.size() * 0.002f);
        float hi = std::max(lo, mFrame.size() * 2.5f);
        return glm::clamp(d, lo, hi);
    }

    // Keep the orbit point inside the frozen area, on the ground.
    void clampTarget() {
        mCam.target.x = glm::clamp(mCam.target.x, mFrame.minLocal.x, mFrame.maxLocal.x);
        mCam.target.y = glm::clamp(mCam.target.y, mFrame.minLocal.y, mFrame.maxLocal.y);
        mCam.target.z = mFrame.groundHeight(mCam.target.x, mCam.target.y);
    }

    // Eases heading and tilt back to north-up, top-down.
    void startAnimation(bool exitAfter) {
        if (mMode != Mode::Scene3D || busy()) return;
        mAnim.active = true;
        mAnim.exitAfter = exitAfter;
        mAnim.startTime = glfwGetTime();
        // Shortest way round: heading in (-180, 180].
        mAnim.startHeading = mCam.heading > 180.0f ? mCam.heading - 360.0f : mCam.heading;
        mAnim.startPitch = mCam.pitch;
        mDragMode = DragMode::None;
        updateControls();
    }

    void updateAnimation() {
        if (!mAnim.active) return;
        double t = (glfwGetTime() - mAnim.startTime) / config::kAnimSeconds;
        if (t > 1.0) t = 1.0;
        float s = float(t * t * (3.0 - 2.0 * t));  // smoothstep
        float heading = mAnim.startHeading * (1.0f - s);
        mCam.heading = heading < 0.0f ? heading + 360.0f : heading;
        mCam.pitch = mAnim.startPitch * (1.0f - s);

        if (t >= 1.0) {
            mAnim.active = false;
            mCam.heading = 0.0f;
            mCam.pitch = 0.0f;
            if (mAnim.exitAfter) finishExit3D();
            updateControls();
        }
    }

    void draw3D() {
        const float aspect = float(size().x()) / float(size().y());
        const glm::mat4 viewProj = mCam.projection(aspect, mFrame.size()) * mCam.view();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);

        // Ground: imagery draped over the terrain meshes (local meters).
        mTerrainShader.begin(1.0f);
        mTerrainShader.setMVP(viewProj);
        for (size_t i = 0; i < mTiles.size(); i++) {
            mTiles[i]->bindTexture();
            mTiles[i]->drawMesh();
        }

        for (size_t i = 0; i < mLayers.size(); i++) {
            if (mLayers[i]->visible && mLayers[i]->loaded) mLayers[i]->draw(viewProj);
        }

        pickUnderCursor(viewProj);
    }

    // Reads the depth buffer under the cursor and turns it back into a
    // scene position, so hover lat/lon is right on terrain and rooftops.
    void pickUnderCursor(const glm::mat4& viewProj) {
        mCursorHit = false;
        if (!mCursorValid) return;

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(glfwWindow(), &fbW, &fbH);
        if (fbW <= 0 || fbH <= 0) return;

        int px = int(mCursorPx.x() * float(fbW) / size().x());
        int py = fbH - 1 - int(mCursorPx.y() * float(fbH) / size().y());
        if (px < 0 || py < 0 || px >= fbW || py >= fbH) return;

        float depth = 1.0f;
        glReadPixels(px, py, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        if (depth >= 1.0f) return;  // nothing there

        glm::vec4 ndc(2.0f * (px + 0.5f) / fbW - 1.0f, 2.0f * (py + 0.5f) / fbH - 1.0f, 2.0f * depth - 1.0f, 1.0f);
        glm::vec4 world = glm::inverse(viewProj) * ndc;
        mCursorWorld = glm::vec3(world) / world.w;
        mCursorHit = true;
    }

    // --- Overlays (NanoVG) ------------------------------------------------------

    // 2D: outline of what the last Render covers. Both modes: crosshair at
    // the view center (the zoom point in 2D, the orbit point in 3D).
    void drawMapOverlay(NVGcontext* ctx) {
        if (mMode == Mode::Map2D && mHaveRender) {
            glm::dvec2 a = mercToScreen(mRenderedCoverage.minX, mRenderedCoverage.minY);
            glm::dvec2 b = mercToScreen(mRenderedCoverage.maxX, mRenderedCoverage.maxY);
            nvgBeginPath(ctx);
            nvgRect(ctx, float(a.x), float(a.y), float(b.x - a.x), float(b.y - a.y));
            nvgStrokeColor(ctx, nvgRGBA(255, 200, 0, 180));
            nvgStrokeWidth(ctx, 1.5f);
            nvgStroke(ctx);
        }

        float cx = size().x() * 0.5f;
        float cy = size().y() * 0.5f;
        nvgBeginPath(ctx);
        nvgMoveTo(ctx, cx - 8, cy);
        nvgLineTo(ctx, cx + 8, cy);
        nvgMoveTo(ctx, cx, cy - 8);
        nvgLineTo(ctx, cx, cy + 8);
        nvgStrokeColor(ctx, nvgRGBA(255, 255, 255, 200));
        nvgStrokeWidth(ctx, 1.5f);
        nvgStroke(ctx);
    }

    struct HudRow { std::string label; std::string value; NVGcolor color; };

    // Info box in the bottom-left corner.
    void drawHud(NVGcontext* ctx) {
        const NVGcolor normal = nvgRGBA(235, 235, 235, 255);
        const NVGcolor good = nvgRGBA(120, 220, 120, 255);
        const NVGcolor warn = nvgRGBA(255, 200, 0, 255);
        const NVGcolor bad = nvgRGBA(255, 110, 110, 255);
        const NVGcolor hint = nvgRGBA(170, 170, 170, 255);

        std::vector<HudRow> rows;
        char buf[160];

        if (mMode == Mode::Map2D) {
            rows.push_back({ "Mode", "2D map", normal });

            std::string cursor = "-";
            if (mCursorValid) {
                glm::dvec2 m = screenToMerc(mCursorPx);
                if (m.x >= 0.0 && m.x <= 1.0 && m.y >= 0.0 && m.y <= 1.0)
                    cursor = geo::format({ geo::mercYToLat(m.y), geo::mercXToLon(m.x) });
            }
            rows.push_back({ "Cursor", cursor, normal });
            rows.push_back({ "Center", geo::format(centerLatLon()), normal });

            std::snprintf(buf, sizeof(buf), "%.2f   (Render loads z%d)", mZoom, tileZoom());
            rows.push_back({ "Zoom", buf, normal });

            std::snprintf(buf, sizeof(buf), "%.2f m/px", metersPerPixel());
            rows.push_back({ "Scale", buf, normal });

            geo::MercBounds v = geo::clampToWorld(visibleBounds());
            std::snprintf(buf, sizeof(buf), "N %.5f    S %.5f", geo::mercYToLat(v.minY), geo::mercYToLat(v.maxY));
            rows.push_back({ "BBox", buf, normal });
            std::snprintf(buf, sizeof(buf), "W %.5f    E %.5f", geo::mercXToLon(v.minX), geo::mercXToLon(v.maxX));
            rows.push_back({ "", buf, normal });

            if (!mHaveRender) {
                rows.push_back({ "Rendered", "nothing yet", warn });
            } else {
                bool upToDate = renderMatchesView();
                std::snprintf(buf, sizeof(buf), "z%d, %d tiles  %s", mRenderedZoom, (int)mTiles.size(),
                              upToDate ? "(up to date)" : "(view changed)");
                rows.push_back({ "Rendered", buf, upToDate ? good : warn });
            }
            rows.push_back({ "", "Drag: pan    Scroll: zoom", hint });
            rows.push_back({ "", "Imagery: Esri World Imagery", hint });
        } else {
            std::snprintf(buf, sizeof(buf), "3D view (frozen at z%d)", mFrame.zoom);
            rows.push_back({ "Mode", buf, good });

            rows.push_back({ "Cursor", mCursorHit ? geo::format(mFrame.toLatLon(mCursorWorld)) : "-", normal });

            // Real elevation of the ground under the cursor, plus height of
            // whatever object is under it.
            std::string elev = "-";
            if (mCursorHit) {
                float above = mCursorWorld.z - mFrame.groundHeight(mCursorWorld.x, mCursorWorld.y);
                elev = "";
                if (mFrame.hasTerrain()) {
                    std::snprintf(buf, sizeof(buf), "%.0f m", mFrame.elevationAt(mCursorWorld.x, mCursorWorld.y));
                    elev = buf;
                }
                if (above > 0.5f) {
                    std::snprintf(buf, sizeof(buf), "%s+%.1f m above ground", elev.empty() ? "" : "   ", above);
                    elev += buf;
                }
                if (elev.empty()) elev = "-";
            }
            rows.push_back({ "Elevation", elev, normal });

            rows.push_back({ "Target", geo::format(mFrame.toLatLon(mCam.target)), normal });

            std::snprintf(buf, sizeof(buf), "heading %.0f\xC2\xB0    tilt %.0f\xC2\xB0", mCam.heading, mCam.pitch);
            rows.push_back({ "View", buf, normal });

            std::snprintf(buf, sizeof(buf), "%.2f m/px at target", mCam.metersPerPixel(float(size().y())));
            rows.push_back({ "Scale", buf, normal });

            if (mFrame.hasTerrain()) {
                std::snprintf(buf, sizeof(buf), "%.0f - %.0f m   (x%.1f, %s)", mElevation.minHeight(),
                              mElevation.maxHeight(), mFrame.exaggeration, mElevation.label().c_str());
                rows.push_back({ "Terrain", buf, normal });
            } else {
                rows.push_back({ "Terrain", "flat", normal });
            }

            const geo::LatLonBox& c = mFrame.latLon;
            std::snprintf(buf, sizeof(buf), "N %.5f    S %.5f", c.north, c.south);
            rows.push_back({ "Scene", buf, normal });
            std::snprintf(buf, sizeof(buf), "W %.5f    E %.5f", c.west, c.east);
            rows.push_back({ "", buf, normal });

            rows.push_back({ "", "Drag: orbit    Shift/right-drag: pan", hint });
            rows.push_back({ "", "Scroll: zoom    Esc: back to 2D", hint });
            const bool usgs = mElevation.dataset().compare(0, 4, "USGS") == 0;
            rows.push_back({ "", std::string("Esri imagery, ") + (usgs ? "USGS 3DEP via " : "") +
                                     "OpenTopography, \xC2\xA9 OpenStreetMap contributors", hint });
        }

        if (!mStatus.empty())
            rows.push_back({ "", mStatus, mStatusIsError ? bad : normal });

        const float pad = 10.0f, lineH = 20.0f, labelW = 80.0f, boxW = 450.0f;

        // Long values (status messages, credits) wrap within the value column.
        nvgFontSize(ctx, 16.0f);
        nvgFontFace(ctx, "sans");
        std::vector<HudRow> lines;
        for (size_t i = 0; i < rows.size(); i++) {
            std::vector<std::string> wrapped = wrapText(ctx, rows[i].value, boxW - 2.0f * pad - labelW);
            for (size_t k = 0; k < wrapped.size(); k++) {
                lines.push_back({ k == 0 ? rows[i].label : "", wrapped[k], rows[i].color });
            }
        }

        const float boxH = pad * 2.0f + lineH * lines.size();
        const float x = 15.0f;
        const float y = size().y() - boxH - 15.0f;

        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, boxW, boxH, 6.0f);
        nvgFillColor(ctx, nvgRGBA(20, 20, 22, 210));
        nvgFill(ctx);

        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        for (size_t i = 0; i < lines.size(); i++) {
            float ty = y + pad + lineH * i;
            nvgFontFace(ctx, "sans-bold");
            nvgFillColor(ctx, nvgRGBA(160, 160, 160, 255));
            nvgText(ctx, x + pad, ty, lines[i].label.c_str(), nullptr);
            nvgFontFace(ctx, "sans");
            nvgFillColor(ctx, lines[i].color);
            nvgText(ctx, x + pad + labelW, ty, lines[i].value.c_str(), nullptr);
        }
    }

    // --- State ----------------------------------------------------------------

    Mode mMode = Mode::Map2D;

    // GL resources
    gfx::TerrainShader mTerrainShader;
    gfx::ColorShader mColorShader;
    gfx::UnitQuad mUnitQuad;

    // 2D camera
    glm::dvec2 mCenter;   // Web Mercator, [0,1]
    double mZoom = 0.0;   // fractional; Render uses tileZoom()

    // Last render
    std::vector<std::unique_ptr<ImageryTile>> mTiles;
    bool mHaveRender = false;
    int mRenderedZoom = 0;
    geo::MercBounds mRenderedCoverage = { 0, 0, 0, 0 };

    // Deferred slow work (see runAfterNotice)
    std::function<void()> mPendingTask;
    bool mNoticeDrawn = false;

    // Terrain settings (apply in 3D)
    bool mUseElevation = true;
    float mExaggeration = 1.0f;

    // 3D scene
    std::vector<std::unique_ptr<MapLayer>> mLayers;
    ElevationGrid mElevation;
    bool mElevationTried = false;  // terrain load attempted for this 3D session
    SceneFrame mFrame;
    OrbitCamera mCam;
    CameraAnimation mAnim;
    bool mCursorHit = false;
    glm::vec3 mCursorWorld = glm::vec3(0.0f);

    // Input
    Eigen::Vector2i mCursorPx = Eigen::Vector2i(0, 0);
    bool mCursorValid = false;
    DragMode mDragMode = DragMode::None;
    int mDragButton = -1;
    Eigen::Vector2i mDragStartPx = Eigen::Vector2i(0, 0);
    Eigen::Vector2i mLastDragPx = Eigen::Vector2i(0, 0);
    glm::dvec2 mDragStartCenter;
    float mDragStartHeading = 0.0f;
    float mDragStartPitch = 0.0f;

    // Location box
    geo::LatLon mPendingLocation = { 0.0, 0.0 };
    bool mPendingValid = true;

    // Widgets
    nanogui::TextBox* mCoordBox = nullptr;
    nanogui::Button* mGoButton = nullptr;
    nanogui::Button* mZoomInButton = nullptr;
    nanogui::Button* mZoomOutButton = nullptr;
    nanogui::Button* mRenderButton = nullptr;
    nanogui::Button* mModeButton = nullptr;
    nanogui::Button* mTopViewButton = nullptr;
    nanogui::CheckBox* mElevationBox = nullptr;
    nanogui::Slider* mExaggerationSlider = nullptr;
    nanogui::Label* mExaggerationLabel = nullptr;
    std::vector<nanogui::CheckBox*> mLayerBoxes;

    // Status line in the HUD
    std::string mStatus;
    bool mStatusIsError = false;
};
}

int MapViewer::run() {
    // Fine if these already exist. mkdir makes one level, so parents first.
    const std::string dirs[] = { config::kCacheDir, config::kImageryCacheDir, config::kTerrainDir,
                                 config::kOsmDir };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) mkdir(dirs[i].c_str(), 0755);

    // Must run once before any thread uses curl (tiles download in parallel).
    curl_global_init(CURL_GLOBAL_DEFAULT);

    int result = 0;
    try {
        nanogui::init();

        /* scoped so the window is destroyed before shutdown */ {
            nanogui::ref<MapViewerScreen> app = new MapViewerScreen();
            app->drawAll();
            app->setVisible(true);
            nanogui::mainloop();
        }

        nanogui::shutdown();
    } catch (const std::exception& e) {
        std::cerr << "Caught a fatal error: " << e.what() << std::endl;
        result = -1;
    }

    curl_global_cleanup();
    return result;
}
