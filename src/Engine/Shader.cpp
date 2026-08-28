/*
 * Shader.cpp — GLSL compile/link wrapper (Phase 8b).
 */
#include "Shader.h"
#include "GpuInit.h"
#include "ShaderManager.h"
#include "Shaders/embedded.h"
#include "Logger.h"

#ifdef __EMSCRIPTEN__
#  include <GLES3/gl3.h>
#  include <chrono>
#endif

namespace OpenXcom
{

/* ── platform GLSL preamble ─────────────────────────────────────────────── */

const char* Shader::preamble()
{
#ifdef __EMSCRIPTEN__
    /* WebGL2 requires explicit version + precision qualifiers.
     * Shader source files must NOT repeat these — the preamble adds them. */
    return "#version 300 es\n"
           "precision highp float;\n"
           "precision highp sampler2D;\n";
#else
    return "#version 330 core\n";
#endif
}

/* ── construction / destruction ─────────────────────────────────────────── */

Shader::Shader()
{
    ShaderManager::instance().registerShader(this);
}

Shader::~Shader()
{
    ShaderManager::instance().unregisterShader(this);
    release();
}

/* ── internal helpers ───────────────────────────────────────────────────── */

static unsigned compileStage(unsigned type, const char* pre, const char* src,
                              long long& msOut)
{
#ifdef __EMSCRIPTEN__
    auto t0 = std::chrono::steady_clock::now();
    const char* srcs[2] = { pre, src };
    GLuint sh = glCreateShader((GLenum)type);
    glShaderSource(sh, 2, srcs, nullptr);
    glCompileShader(sh);
    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    auto t1 = std::chrono::steady_clock::now();
    msOut = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    if (!ok)
    {
        GLint len = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log(len > 1 ? (size_t)len : 1u, '\0');
        if (len > 0) glGetShaderInfoLog(sh, len, nullptr, &log[0]);
        Log(LOG_ERROR) << "Shader compile error: " << log;
        glDeleteShader(sh);
        return 0u;
    }
    return (unsigned)sh;
#else
    (void)type; (void)pre; (void)src; (void)msOut;
    return 0u;
#endif
}

bool Shader::compile(const char* vertSrc, const char* fragSrc)
{
#ifdef __EMSCRIPTEN__
    release();
    if (!GpuInit::ready()) return false;

    const char* pre = preamble();
    long long vMs = 0, fMs = 0;
    unsigned vs = compileStage(GL_VERTEX_SHADER,   pre, vertSrc, vMs);
    unsigned fs = compileStage(GL_FRAGMENT_SHADER, pre, fragSrc, fMs);
    if (!vs || !fs)
    {
        if (vs) glDeleteShader((GLuint)vs);
        if (fs) glDeleteShader((GLuint)fs);
        return false;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, (GLuint)vs);
    glAttachShader(prog, (GLuint)fs);
    glLinkProgram(prog);
    glDeleteShader((GLuint)vs);
    glDeleteShader((GLuint)fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len > 1 ? (size_t)len : 1u, '\0');
        if (len > 0) glGetProgramInfoLog(prog, len, nullptr, &log[0]);
        Log(LOG_ERROR) << "Shader link error: " << log;
        glDeleteProgram(prog);
        return false;
    }

    _program = (unsigned)prog;
    Log(LOG_INFO) << "shader '" << (_name.empty() ? "<anon>" : _name) << "' compiled in " << (vMs + fMs) << " ms";
    return true;
#else
    (void)vertSrc; (void)fragSrc;
    return false;
#endif
}

void Shader::release()
{
#ifdef __EMSCRIPTEN__
    if (_program) { glDeleteProgram((GLuint)_program); _program = 0u; }
#endif
    _uniformCache.clear();
}

/* ── public API ─────────────────────────────────────────────────────────── */

bool Shader::loadFromSource(const char* vertSrc, const char* fragSrc)
{
    _vertSrc = vertSrc ? vertSrc : "";
    _fragSrc = fragSrc ? fragSrc : "";
    return compile(vertSrc, fragSrc);
}

bool Shader::loadFromEmbedded(const char* name)
{
    Shaders::ShaderPair p = Shaders::findEmbedded(name);
    if (!p.vert || !p.frag)
    {
        Log(LOG_ERROR) << "Shader::loadFromEmbedded: unknown shader '" << name << "'";
        return false;
    }
    _name    = name ? name : "";
    _vertSrc = p.vert;
    _fragSrc = p.frag;
    return compile(p.vert, p.frag);
}

void Shader::use()
{
#ifdef __EMSCRIPTEN__
    if (_program) glUseProgram((GLuint)_program);
#endif
}

int Shader::getUniformLocation(const char* name)
{
#ifdef __EMSCRIPTEN__
    if (!_program) return -1;
    auto it = _uniformCache.find(name);
    if (it != _uniformCache.end()) return it->second;
    int loc = glGetUniformLocation((GLuint)_program, name);
    _uniformCache[name] = loc;
    return loc;
#else
    (void)name; return -1;
#endif
}

void Shader::setUniform1f(const char* n, float v)
{
#ifdef __EMSCRIPTEN__
    int l = getUniformLocation(n); if (l >= 0) glUniform1f(l, v);
#else
    (void)n; (void)v;
#endif
}

void Shader::setUniform2f(const char* n, float v0, float v1)
{
#ifdef __EMSCRIPTEN__
    int l = getUniformLocation(n); if (l >= 0) glUniform2f(l, v0, v1);
#else
    (void)n; (void)v0; (void)v1;
#endif
}

void Shader::setUniform3f(const char* n, float v0, float v1, float v2)
{
#ifdef __EMSCRIPTEN__
    int l = getUniformLocation(n); if (l >= 0) glUniform3f(l, v0, v1, v2);
#else
    (void)n; (void)v0; (void)v1; (void)v2;
#endif
}

void Shader::setUniform4f(const char* n, float v0, float v1, float v2, float v3)
{
#ifdef __EMSCRIPTEN__
    int l = getUniformLocation(n); if (l >= 0) glUniform4f(l, v0, v1, v2, v3);
#else
    (void)n; (void)v0; (void)v1; (void)v2; (void)v3;
#endif
}

void Shader::setUniform1i(const char* n, int v)
{
#ifdef __EMSCRIPTEN__
    int l = getUniformLocation(n); if (l >= 0) glUniform1i(l, v);
#else
    (void)n; (void)v;
#endif
}

void Shader::setUniformMat3(const char* n, const float* m)
{
#ifdef __EMSCRIPTEN__
    int l = getUniformLocation(n); if (l >= 0) glUniformMatrix3fv(l, 1, GL_FALSE, m);
#else
    (void)n; (void)m;
#endif
}

void Shader::setUniformMat4(const char* n, const float* m)
{
#ifdef __EMSCRIPTEN__
    int l = getUniformLocation(n); if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, m);
#else
    (void)n; (void)m;
#endif
}

bool Shader::reupload()
{
    /* The stale program belongs to the replaced GL context; its handle is
     * meaningless here — abandon it without deleting, then rebuild fresh. */
    _program = 0u;
    _uniformCache.clear();
    if (!_vertSrc.empty() && !_fragSrc.empty())
        return compile(_vertSrc.c_str(), _fragSrc.c_str());
    return true; // A never-loaded Shader owns no recoverable GPU resource.
}

} // namespace OpenXcom
