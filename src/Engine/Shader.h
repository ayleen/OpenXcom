#pragma once
/*
 * Shader — GLSL program compile/link wrapper (Phase 8b).
 *
 * Usage:
 *   Shader sh;
 *   sh.loadFromEmbedded("colorquad");   // or loadFromSource(vertSrc, fragSrc)
 *   sh.use();
 *   sh.setUniform4f("u_color", 1,0,0,1);
 *
 * Instances register themselves with ShaderManager for lost-context recovery.
 * The platform GLSL preamble (#version / precision) is prepended automatically.
 */
#include <string>
#include <unordered_map>

namespace OpenXcom
{
class Shader
{
public:
    Shader();
    ~Shader();

    bool loadFromSource(const char* vertSrc, const char* fragSrc);
    bool loadFromEmbedded(const char* name);

    void use();

    void setUniform1f(const char* name, float v);
    void setUniform2f(const char* name, float v0, float v1);
    void setUniform3f(const char* name, float v0, float v1, float v2);
    void setUniform4f(const char* name, float v0, float v1, float v2, float v3);
    void setUniform1i(const char* name, int v);
    void setUniformMat3(const char* name, const float* m);
    void setUniformMat4(const char* name, const float* m);
    int  getUniformLocation(const char* name);

    bool isValid() const { return _program != 0u; }

    /* Called by ShaderManager on SDL_RENDER_TARGETS_RESET. */
    void reupload();

private:
    unsigned _program = 0u;
    std::string _name;
    std::string _vertSrc;
    std::string _fragSrc;
    std::unordered_map<std::string, int> _uniformCache;

    static const char* preamble();
    bool compile(const char* vertSrc, const char* fragSrc);
    void release();
};
} // namespace OpenXcom
