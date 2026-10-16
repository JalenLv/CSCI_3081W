// Private copy of stb_image for decoding tiles. STB_IMAGE_STATIC keeps every
// function local to this file, so it can't clash with the copies inside
// ggis_graphics and NanoGUI. Must come before any other include. This is the
// only file that decodes images, so it's the only copy in the app.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#include "Graphics.h"

#include <stdexcept>

#include <glm/gtc/type_ptr.hpp>

#include "Cache.h"

namespace gfx {

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error(std::string("Shader compile error: ") + log);
    }
    return s;
}

GLuint linkProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        glDeleteProgram(prog);
        throw std::runtime_error(std::string("Shader link error: ") + log);
    }
    return prog;
}

// --- TerrainShader ---------------------------------------------------------

namespace {
const char* const kTerrainVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
uniform mat4 uMVP;
out vec2 vUV;
out vec3 vNormal;
void main() {
    vUV = aUV;
    vNormal = aNormal;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* const kTerrainFragmentShader = R"(
#version 330 core
in vec2 vUV;
in vec3 vNormal;
uniform sampler2D uTexture;
uniform float uShade;
out vec4 fragColor;
void main() {
    vec3 lightDir = normalize(vec3(0.35, 0.55, 0.75));
    float diffuse = max(dot(normalize(vNormal), lightDir), 0.0);
    float light = mix(1.0, 0.55 + 0.6 * diffuse, uShade);
    fragColor = vec4(texture(uTexture, vUV).rgb * light, 1.0);
}
)";

const char* const kColorVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
uniform mat4 uViewProj;
out vec3 vColor;
void main() {
    vec3 lightDir = normalize(vec3(0.35, 0.55, 0.75));
    float diffuse = max(dot(normalize(aNormal), lightDir), 0.0);
    vColor = aColor * (0.45 + 0.55 * diffuse);
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

const char* const kColorFragmentShader = R"(
#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() {
    fragColor = vec4(vColor, 1.0);
}
)";
}

void TerrainShader::init() {
    mProgram = linkProgram(kTerrainVertexShader, kTerrainFragmentShader);
    mMVPLoc = glGetUniformLocation(mProgram, "uMVP");
    mShadeLoc = glGetUniformLocation(mProgram, "uShade");
    glUseProgram(mProgram);
    glUniform1i(glGetUniformLocation(mProgram, "uTexture"), 0);
    glUseProgram(0);
}

void TerrainShader::destroy() {
    if (mProgram) glDeleteProgram(mProgram);
    mProgram = 0;
}

void TerrainShader::begin(float shade) const {
    glUseProgram(mProgram);
    glUniform1f(mShadeLoc, shade);
    glActiveTexture(GL_TEXTURE0);
}

void TerrainShader::setMVP(const glm::mat4& mvp) const {
    glUniformMatrix4fv(mMVPLoc, 1, GL_FALSE, glm::value_ptr(mvp));
}

// --- ColorShader -----------------------------------------------------------

void ColorShader::init() {
    mProgram = linkProgram(kColorVertexShader, kColorFragmentShader);
    mViewProjLoc = glGetUniformLocation(mProgram, "uViewProj");
}

void ColorShader::destroy() {
    if (mProgram) glDeleteProgram(mProgram);
    mProgram = 0;
}

void ColorShader::use(const glm::mat4& viewProj) const {
    glUseProgram(mProgram);
    glUniformMatrix4fv(mViewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProj));
}

// --- ColorMesh -------------------------------------------------------------

void ColorMesh::upload(GLenum primitive) {
    mPrimitive = primitive;
    mCount = GLsizei(mData.size() / 9);
    if (mCount == 0) return;

    glGenVertexArrays(1, &mVao);
    glGenBuffers(1, &mVbo);
    glBindVertexArray(mVao);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, mData.size() * sizeof(float), mData.data(), GL_STATIC_DRAW);
    const GLsizei stride = 9 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    mData.clear();
    mData.shrink_to_fit();
}

void ColorMesh::draw() const {
    if (mCount == 0) return;
    glBindVertexArray(mVao);
    glDrawArrays(mPrimitive, 0, mCount);
    glBindVertexArray(0);
}

void ColorMesh::clear() {
    if (mVbo) glDeleteBuffers(1, &mVbo);
    if (mVao) glDeleteVertexArrays(1, &mVao);
    mVbo = mVao = 0;
    mCount = 0;
    mData.clear();
}

// --- UnitQuad --------------------------------------------------------------

void UnitQuad::init() {
    const float v[] = {
        0, 0, 0,  0, 0,  0, 0, 1,
        1, 0, 0,  1, 0,  0, 0, 1,
        1, 1, 0,  1, 1,  0, 0, 1,
        0, 0, 0,  0, 0,  0, 0, 1,
        1, 1, 0,  1, 1,  0, 0, 1,
        0, 1, 0,  0, 1,  0, 0, 1,
    };
    glGenVertexArrays(1, &mVao);
    glGenBuffers(1, &mVbo);
    glBindVertexArray(mVao);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void UnitQuad::destroy() {
    if (mVbo) glDeleteBuffers(1, &mVbo);
    if (mVao) glDeleteVertexArrays(1, &mVao);
    mVbo = mVao = 0;
}

void UnitQuad::draw() const {
    glBindVertexArray(mVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
}

// --- ImageryTile -------------------------------------------------------------

ImageryTile::~ImageryTile() {
    freeMesh();
    if (mTexture) glDeleteTextures(1, &mTexture);
}

bool ImageryTile::setImage(const std::string& bytes) {
    if (!tiles::looksLikeImage(bytes)) return false;
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load_from_memory((const stbi_uc*)bytes.data(), int(bytes.size()), &w, &h, &comp, 4);
    if (!px) return false;

    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);  // keeps tilted views from shimmering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(px);
    return true;
}

void ImageryTile::buildMesh(const SceneFrame& frame, int cells) {
    freeMesh();
    const double n = double(1 << z);
    const float cellMeters = float(frame.metersPerMerc / n / cells);

    std::vector<float> verts;
    verts.reserve(size_t(cells + 1) * (cells + 1) * 8);
    for (int j = 0; j <= cells; j++) {
        for (int i = 0; i <= cells; i++) {
            const float u = float(i) / cells, v = float(j) / cells;  // v = 0 is the north edge
            const double mx = (x + u) / n, my = (y + v) / n;
            const float lx = float((mx - frame.originMerc.x) * frame.metersPerMerc);
            const float ly = float(-(my - frame.originMerc.y) * frame.metersPerMerc);
            const float h = frame.groundHeight(lx, ly);

            glm::vec3 normal(0.0f, 0.0f, 1.0f);
            if (frame.hasTerrain()) {
                const float e = cellMeters;
                float dhdx = (frame.groundHeight(lx + e, ly) - frame.groundHeight(lx - e, ly)) / (2.0f * e);
                float dhdy = (frame.groundHeight(lx, ly + e) - frame.groundHeight(lx, ly - e)) / (2.0f * e);
                normal = glm::normalize(glm::vec3(-dhdx, -dhdy, 1.0f));
            }

            const float vtx[8] = { lx, ly, h, u, v, normal.x, normal.y, normal.z };
            verts.insert(verts.end(), vtx, vtx + 8);
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(size_t(cells) * cells * 6);
    for (int j = 0; j < cells; j++) {
        for (int i = 0; i < cells; i++) {
            uint32_t a = uint32_t(j * (cells + 1) + i), b = a + 1;
            uint32_t c = a + uint32_t(cells + 1), d = c + 1;
            indices.push_back(a); indices.push_back(c); indices.push_back(b);
            indices.push_back(b); indices.push_back(c); indices.push_back(d);
        }
    }

    glGenVertexArrays(1, &mVao);
    glGenBuffers(1, &mVbo);
    glGenBuffers(1, &mEbo);
    glBindVertexArray(mVao);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    mIndexCount = GLsizei(indices.size());
}

void ImageryTile::drawMesh() const {
    if (!mIndexCount) return;
    glBindVertexArray(mVao);
    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, (void*)0);
    glBindVertexArray(0);
}

void ImageryTile::freeMesh() {
    if (mEbo) glDeleteBuffers(1, &mEbo);
    if (mVbo) glDeleteBuffers(1, &mVbo);
    if (mVao) glDeleteVertexArrays(1, &mVao);
    mEbo = mVbo = mVao = 0;
    mIndexCount = 0;
}
