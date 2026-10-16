#ifndef MAPVIEWER_GRAPHICS_H_
#define MAPVIEWER_GRAPHICS_H_

// GL helpers (shaders, colored meshes for 3D layers, the 2D unit quad) and
// ImageryTile. Include this before any NanoGUI/GLFW header: it pulls in glad,
// which must come before anything that includes GL/gl.h.

#include <glad/glad.h>

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Scene.h"

namespace gfx {

GLuint compileShader(GLenum type, const char* src);
GLuint linkProgram(const char* vsSrc, const char* fsSrc);

// Imagery: textured surface with optional hillshading (0 in 2D, 1 in 3D).
// Flat ground comes out at the texture's original brightness.
class TerrainShader {
public:
    void init();
    void destroy();
    void begin(float shade) const;
    void setMVP(const glm::mat4& mvp) const;

private:
    GLuint mProgram = 0;
    GLint mMVPLoc = -1, mShadeLoc = -1;
};

// 3D layers: per-vertex position, normal and color with a directional light.
class ColorShader {
public:
    void init();
    void destroy();
    void use(const glm::mat4& viewProj) const;

private:
    GLuint mProgram = 0;
    GLint mViewProjLoc = -1;
};

// Interleaved position/normal/color vertices, uploaded once, drawn as
// triangles or lines. add*() only touch CPU memory, so geometry can be built
// (and tested) without a GL context; upload() needs one.
class ColorMesh {
public:
    ColorMesh() {}
    ~ColorMesh() { clear(); }
    ColorMesh(const ColorMesh&) = delete;
    ColorMesh& operator=(const ColorMesh&) = delete;

    void addVertex(const glm::vec3& p, const glm::vec3& n, const glm::vec3& c) {
        const float v[9] = { p.x, p.y, p.z, n.x, n.y, n.z, c.x, c.y, c.z };
        mData.insert(mData.end(), v, v + 9);
    }

    void addTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                     const glm::vec3& normal, const glm::vec3& color) {
        addVertex(a, normal, color); addVertex(b, normal, color); addVertex(c, normal, color);
    }

    // Two triangles (a,b,c) and (a,c,d) sharing one normal.
    void addQuad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                 const glm::vec3& normal, const glm::vec3& color) {
        addTriangle(a, b, c, normal, color);
        addTriangle(a, c, d, normal, color);
    }

    void addLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& colorA, const glm::vec3& colorB) {
        const glm::vec3 up(0.0f, 0.0f, 1.0f);
        addVertex(a, up, colorA);
        addVertex(b, up, colorB);
    }

    // Vertices added but not uploaded yet.
    size_t pendingVertices() const { return mData.size() / 9; }

    void upload(GLenum primitive);
    void draw() const;
    void clear();

private:
    std::vector<float> mData;
    GLuint mVao = 0, mVbo = 0;
    GLsizei mCount = 0;
    GLenum mPrimitive = GL_TRIANGLES;
};

// A unit quad (0..1 in x/y, uv = xy) for drawing tiles flat in 2D.
// Vertex layout matches the terrain shader: position, uv, normal.
class UnitQuad {
public:
    void init();
    void destroy();
    void draw() const;

private:
    GLuint mVao = 0, mVbo = 0;
};
}

// ---------------------------------------------------------------------------
// One imagery tile: its texture, plus (in 3D) a grid mesh draped over the
// terrain. Neighboring tiles compute identical edge vertices, so there are
// no cracks between them.
// ---------------------------------------------------------------------------

class ImageryTile {
public:
    ImageryTile(int x, int y, int z) : x(x), y(y), z(z) {}
    ~ImageryTile();
    ImageryTile(const ImageryTile&) = delete;
    ImageryTile& operator=(const ImageryTile&) = delete;

    // Decodes JPEG/PNG bytes into a mipmapped texture.
    bool setImage(const std::string& bytes);

    void bindTexture() const { glBindTexture(GL_TEXTURE_2D, mTexture); }

    // Builds an N x N grid over this tile in local meters, raised to the
    // frame's ground height, with normals for hillshading.
    void buildMesh(const SceneFrame& frame, int cells);

    void drawMesh() const;
    void freeMesh();

    const int x, y, z;

private:
    GLuint mTexture = 0;
    GLuint mVao = 0, mVbo = 0, mEbo = 0;
    GLsizei mIndexCount = 0;
};

#endif
