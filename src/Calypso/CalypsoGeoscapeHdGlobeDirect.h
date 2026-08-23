#pragma once

#ifdef __EMSCRIPTEN__
#include <cmath>
#include <memory>
#include <string>
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Mod/Mod.h"

/* Included from Geoscape/Globe.cpp after Globe.h: needs the complete type. */
namespace OpenXcom {

struct CalypsoGeoscapeHdGlobeDirect
{

    static void computeSphereRes(const Globe* globe, int& w, int& h)
    {
        w = globe->getWidth(); h = globe->getHeight();
        if (globe->_gpuDirectMode && globe->_directScreen != nullptr)
        {
            w = std::max(1, (int)std::lround(w * globe->_directScreen->getXScale()));
            h = std::max(1, (int)std::lround(h * globe->_directScreen->getYScale()));
        }
    }

    static void setGpuDirect(Globe* globe, bool on)
    {
        if (on == globe->_gpuDirectMode) return;
        globe->_gpuDirectMode = on;
        globe->_directScreen = on ? globe->_game->getScreen() : nullptr;
        if (!(on && globe->_directScreen)) return;
        SDL_SetColorKey(globe->getSurface(), SDL_SRCCOLORKEY, 0);
        if (!globe->_gpuAliveFlag) globe->_gpuAliveFlag = std::make_shared<bool>(true);
        if (!globe->_gpuSphereOK && !globe->initSphereGPU()) { globe->_gpuDirectMode = false; return; }
        std::weak_ptr<bool> wf = globe->_gpuAliveFlag;
        Screen* screen = globe->_directScreen;
        screen->registerGPUPassPreComposite([globe, wf, screen]() {
            if (!wf.lock()) return;
            CalypsoGeoscapeHdGlobeDirect::drawPass(globe);
        });
    }

    static void drawPass(Globe* globe)
    {
        if (!globe->_gpuDirectMode || !globe->_globeShader || !globe->_directScreen) return;
        Mod* mod = globe->_game->getMod();
        GpuTexture* bathyTex = mod->getGlobeTexture("bathymetry");
        GpuTexture* diffuseTex = mod->getGlobeTexture("diffuse");
        GpuTexture* nightTex = mod->getGlobeTexture("night");
        GpuTexture* cloudsTex = mod->getGlobeTexture("clouds");
        if (!bathyTex || !diffuseTex || !nightTex || !cloudsTex) return;
        int w = 0, h = 0; computeSphereRes(globe, w, h);
        const double xs = globe->_directScreen->getXScale();
        const double ys = globe->_directScreen->getYScale();
        const int lbb = globe->_directScreen->getCursorLeftBlackBand();
        const int tbb = globe->_directScreen->getCursorTopBlackBand();
        const int dW = Options::displayWidth, dH = Options::displayHeight;
        const int dispX = (int)(globe->getX() * xs) + lbb;
        const int dispY = (int)(globe->getY() * ys) + tbb;
        const int dispW = (int)(globe->getWidth() * xs);
        const int dispH = (int)(globe->getHeight() * ys);
        GlobeSphereGlSave st; st.save();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(dispX, dispY, dispW, dispH);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        globe->_globeShader->use();
        bathyTex->bind(0);
        globe->_globeShader->setUniform1i("u_bathymetry", 0);
        diffuseTex->bind(1);
        globe->_globeShader->setUniform1i("u_diffuse", 1);
        nightTex->bind(2);
        globe->_globeShader->setUniform1i("u_night", 2);
        cloudsTex->bind(3);
        globe->_globeShader->setUniform1i("u_clouds", 3);
        globe->_globeShader->setUniform2f("u_viewportSize", (float)dispW, (float)dispH);
        globe->_globeShader->setUniform2f("u_globeCenter", (float)globe->_cenX, (float)globe->_cenY);
        globe->_globeShader->setUniform1f("u_globeRadius", (float)globe->_zoomRadius[globe->_zoom]);
        globe->_globeShader->setUniform1f("u_camLat", (float)globe->_cenLat);
        globe->_globeShader->setUniform1f("u_camLon", (float)globe->_cenLon);
        Cord sd = globe->getSunDirectionWorld();
        globe->_globeShader->setUniform3f("u_sunDirWorld", (float)sd.x, (float)sd.y, (float)sd.z);
        float mipLvl = std::max(0.f, std::min(1.35f, 1.35f - (float)globe->_zoom * 0.27f));
        globe->_globeShader->setUniform1f("u_mipLevel", mipLvl);
        glBindVertexArray(globe->_sphereVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0u);
        for (int i = 3; i >= 0; --i) { glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, 0u); }
        st.restore();
    }
}

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */