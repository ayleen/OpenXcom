/*
 * Phase-42 G0: synthetic RGBA unit-atlas browser spike.
 *
 * Harness-only invariants:
 *  - accepts only a synthetic 4x3 RGBA PNG from MEMFS;
 *  - exercises the production tile_atlas and tile_atlas_rgba shaders;
 *  - uploads an sRGBA texture with a complete mip chain;
 *  - drops its CPU mirror, evicts the GL name, then reloads from MEMFS once;
 *  - writes a GPU screenshot and machine-readable metrics back to MEMFS.
 *
 * It does not inspect rulesets, replace any production atlas, or run natively.
 */
#ifdef __EMSCRIPTEN__

#include "HdUnitSpikeState.h"
#include "GpuInit.h"
#include "GpuTexture.h"
#include "Logger.h"
#include "Screen.h"
#include "Shader.h"
#include "ShaderManager.h"

#include <GLES3/gl3.h>
#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

extern "C" double calypso_heap_used(void);

namespace OpenXcom
{
namespace
{

struct DecodedRgba
{
	int w = 0;
	int h = 0;
	std::vector<uint8_t> pixels;
};

static void sampleHeapPeak(double *peak)
{
	if (peak) *peak = std::max(*peak, calypso_heap_used());
}

static bool decodeRgba(const std::string &path, DecodedRgba &out, std::string &error,
	                   double *heapPeak = nullptr)
{
	SDL_RWops *rw = SDL_RWFromFile(path.c_str(), "rb");
	if (!rw)
	{
		error = std::string("SDL_RWFromFile: ") + SDL_GetError();
		return false;
	}
	SDL_Surface *raw = IMG_Load_RW(rw, SDL_TRUE);
	if (!raw)
	{
		error = std::string("IMG_Load_RW: ") + IMG_GetError();
		return false;
	}
	sampleHeapPeak(heapPeak);
	SDL_Surface *rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
	if (!rgba)
	{
		SDL_FreeSurface(raw);
		error = std::string("SDL_ConvertSurfaceFormat: ") + SDL_GetError();
		return false;
	}
	/* Both decode surfaces coexist here; this is a material WASM peak for the
	 * large-atlas gate and must be sampled before releasing the source. */
	sampleHeapPeak(heapPeak);
	SDL_FreeSurface(raw);

	out.w = rgba->w;
	out.h = rgba->h;
	out.pixels.resize((size_t)out.w * (size_t)out.h * 4u);
	sampleHeapPeak(heapPeak);
	if (SDL_MUSTLOCK(rgba) && SDL_LockSurface(rgba) != 0)
	{
		error = std::string("SDL_LockSurface: ") + SDL_GetError();
		SDL_FreeSurface(rgba);
		return false;
	}
	for (int y = 0; y < out.h; ++y)
	{
		const uint8_t *src = static_cast<const uint8_t *>(rgba->pixels) + (size_t)y * rgba->pitch;
		std::copy(src, src + (size_t)out.w * 4u,
		          out.pixels.begin() + (size_t)y * (size_t)out.w * 4u);
	}
	if (SDL_MUSTLOCK(rgba)) SDL_UnlockSurface(rgba);
	sampleHeapPeak(heapPeak);
	SDL_FreeSurface(rgba);
	return true;
}

static size_t mipChainBytes(int w, int h, int channels)
{
	size_t total = 0;
	for (;;)
	{
		total += (size_t)w * (size_t)h * (size_t)channels;
		if (w == 1 && h == 1) break;
		w = std::max(1, w / 2);
		h = std::max(1, h / 2);
	}
	return total;
}

static std::string jsonEscape(const std::string &s)
{
	std::ostringstream o;
	for (unsigned char c : s)
	{
		switch (c)
		{
		case '\\': o << "\\\\"; break;
		case '"':  o << "\\\""; break;
		case '\n': o << "\\n"; break;
		case '\r': o << "\\r"; break;
		case '\t': o << "\\t"; break;
		default:
			if (c < 0x20) o << "?";
			else o << (char)c;
		}
	}
	return o.str();
}

struct Instance
{
	float x, y, u, v, shade, frames, alpha, iso;
};

struct GlSave
{
	GLint program = 0, vao = 0, arrayBuffer = 0, activeTexture = 0;
	GLint viewport[4] = {};
	GLint scissorBox[4] = {};
	GLint depthFunc = GL_LESS, blendSrcRgb = GL_ONE, blendDstRgb = GL_ZERO;
	GLint blendSrcAlpha = GL_ONE, blendDstAlpha = GL_ZERO;
	GLboolean blend = GL_FALSE, depth = GL_FALSE, scissor = GL_FALSE;
	GLboolean depthMask = GL_TRUE;
	GLboolean colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
	GLfloat clearColor[4] = {};
	GLint texture2d[4] = {};
	GLint originalTexture2d = 0;

	void save()
	{
		glGetIntegerv(GL_CURRENT_PROGRAM, &program);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &originalTexture2d);
		for (int unit = 0; unit < 4; ++unit)
		{
			glActiveTexture(GL_TEXTURE0 + unit);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2d[unit]);
		}
		glActiveTexture((GLenum)activeTexture);
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
		glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
		glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
		glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
		blend = glIsEnabled(GL_BLEND);
		depth = glIsEnabled(GL_DEPTH_TEST);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
		glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
	}

	void restore()
	{
		glUseProgram((GLuint)program);
		glBindVertexArray((GLuint)vao);
		glBindBuffer(GL_ARRAY_BUFFER, (GLuint)arrayBuffer);
		for (int unit = 0; unit < 4; ++unit)
		{
			glActiveTexture(GL_TEXTURE0 + unit);
			glBindTexture(GL_TEXTURE_2D, (GLuint)texture2d[unit]);
		}
		glActiveTexture((GLenum)activeTexture);
		/* init() uploads on whatever unit SDL left active. Preserve it even when
		 * that selector lies outside the units touched by drawOne(). */
		if (activeTexture < GL_TEXTURE0 || activeTexture > GL_TEXTURE3)
			glBindTexture(GL_TEXTURE_2D, (GLuint)originalTexture2d);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
		glDepthFunc((GLenum)depthFunc);
		glBlendFuncSeparate((GLenum)blendSrcRgb, (GLenum)blendDstRgb,
		                    (GLenum)blendSrcAlpha, (GLenum)blendDstAlpha);
		if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
		if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
		if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
		glDepthMask(depthMask);
		glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
	}
};

struct ScopedGlRestore
{
	GlSave &state;
	~ScopedGlRestore() { state.restore(); }
};

struct HdUnitPass
{
	Screen *screen;
	std::string assetPath, outPath, metricsPath;
	Shader r8Shader, rgbaShader;
	GpuTexture rgbaAtlas{true, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Linear};
	GpuTexture r8Atlas{false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest};
	GpuTexture shadeTable{false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest};
	GpuTexture shadeCurve{false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest};
	GLuint vao = 0, cornerVbo = 0, instanceVbo = 0;
	int atlasW = 0, atlasH = 0, cellW = 0, cellH = 0;
	int frames = 0;
	bool ready = false, evicted = false, reuploaded = false;
	double heapBefore = 0, heapAfterUpload = 0, heapAfterReupload = 0;
	double heapDecodePeak = 0, heapReloadPeak = 0, heapPeak = 0;
	GLint maxTextureSize = 0;
	GLenum initGlError = GL_NO_ERROR, renderGlError = GL_NO_ERROR;
	std::string error;

	HdUnitPass(Screen *s, const std::string &asset, const std::string &out,
	           const std::string &metrics)
		: screen(s), assetPath(asset), outPath(out), metricsPath(metrics) {}

	~HdUnitPass()
	{
		if (instanceVbo) glDeleteBuffers(1, &instanceVbo);
		if (cornerVbo) glDeleteBuffers(1, &cornerVbo);
		if (vao) glDeleteVertexArrays(1, &vao);
	}

	bool reloadAtlas()
	{
		DecodedRgba image;
		std::string why;
		if (!decodeRgba(assetPath, image, why, &heapReloadPeak))
		{
			error = why;
			return false;
		}
		if (image.w != atlasW || image.h != atlasH)
		{
			error = "atlas dimensions changed during reload";
			return false;
		}
		bool uploaded = rgbaAtlas.uploadRGBA(image.pixels.data(), image.w, image.h, 0);
		sampleHeapPeak(&heapReloadPeak); // decoded vector is still live here
		heapPeak = std::max(heapPeak, heapReloadPeak);
		return uploaded;
	}

	bool init()
	{
		GlSave entryState;
		entryState.save();
		ScopedGlRestore restoreOnExit{entryState};
		while (glGetError() != GL_NO_ERROR) {}
		heapBefore = calypso_heap_used();
		heapDecodePeak = heapBefore;
		heapReloadPeak = heapBefore;
		heapPeak = heapBefore;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

		if (!r8Shader.loadFromEmbedded("tile_atlas") ||
		    !rgbaShader.loadFromEmbedded("tile_atlas_rgba"))
		{
			error = "production atlas shader compile failed";
			return false;
		}

		DecodedRgba image;
		if (!decodeRgba(assetPath, image, error, &heapDecodePeak)) return false;
		heapPeak = std::max(heapPeak, heapDecodePeak);
		if (image.w <= 0 || image.h <= 0 || image.w % 4 != 0 || image.h % 3 != 0)
		{
			error = "synthetic atlas must have positive dimensions divisible by 4x3";
			return false;
		}
		atlasW = image.w; atlasH = image.h;
		cellW = atlasW / 4; cellH = atlasH / 3;
		if (atlasW > maxTextureSize || atlasH > maxTextureSize)
		{
			error = "synthetic atlas exceeds GL_MAX_TEXTURE_SIZE";
			return false;
		}

		rgbaAtlas.setSkipCache(true);
		if (!rgbaAtlas.uploadRGBA(image.pixels.data(), atlasW, atlasH, 0))
		{
			error = "RGBA upload failed";
			return false;
		}
		heapAfterUpload = calypso_heap_used();
		heapPeak = std::max(heapPeak, heapAfterUpload);
		rgbaAtlas.setReloadCb([this]() { reuploaded = reloadAtlas(); });
		rgbaAtlas.evictGL();
		evicted = !rgbaAtlas.isValid();
		rgbaAtlas.reupload();
		heapAfterReupload = calypso_heap_used();
		heapPeak = std::max(heapPeak, heapAfterReupload);
		if (!evicted || !reuploaded || !rgbaAtlas.isValid())
		{
			if (error.empty()) error = "forced RGBA evict/reupload failed";
			return false;
		}

		/* Procedural palette-index atlas: transparent border, stable non-zero
		 * indices in every source cell. This is the real R8 fallback path. */
		std::vector<uint8_t> r8((size_t)atlasW * atlasH, 0u);
		for (int y = 0; y < atlasH; ++y)
			for (int x = 0; x < atlasW; ++x)
			{
				int lx = x % cellW, ly = y % cellH;
				float nx = (2.0f * lx - cellW) / (float)cellW;
				float ny = (2.0f * ly - cellH) / (float)cellH;
				if (nx * nx + ny * ny < 0.72f)
					r8[(size_t)y * atlasW + x] = (uint8_t)(48 + ((x / cellW) * 37 + (y / cellH) * 29 + ly / 4) % 190);
			}
		if (!r8Atlas.uploadR8(r8.data(), atlasW, atlasH))
		{
			error = "R8 upload failed";
			return false;
		}

		std::vector<uint8_t> table(16u * 256u * 4u, 255u);
		for (int pal = 0; pal < 256; ++pal)
			for (int shade = 0; shade < 16; ++shade)
			{
				float f = 1.0f - 0.055f * shade;
				size_t i = ((size_t)pal * 16u + (size_t)shade) * 4u;
				table[i + 0] = (uint8_t)(pal * f);
				table[i + 1] = (uint8_t)((255 - pal / 2) * f);
				table[i + 2] = (uint8_t)((64 + pal / 2) * f);
				table[i + 3] = 255u;
			}
		if (!shadeTable.uploadRGBA(table.data(), 16, 256, 0))
		{
			error = "shade table upload failed";
			return false;
		}
		uint8_t curve[16];
		for (int s = 0; s < 16; ++s) curve[s] = (uint8_t)(255 - s * 13);
		if (!shadeCurve.uploadR8(curve, 16, 1))
		{
			error = "shade curve upload failed";
			return false;
		}

		static const float corners[12] = {
			0.f, 0.f, 1.f, 0.f, 0.f, 1.f,
			0.f, 1.f, 1.f, 0.f, 1.f, 1.f
		};
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &cornerVbo);
		glBindBuffer(GL_ARRAY_BUFFER, cornerVbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(0);
		glGenBuffers(1, &instanceVbo);
		glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Instance), nullptr, GL_DYNAMIC_DRAW);
		for (GLuint loc = 1; loc <= 6; ++loc)
		{
			GLint count = (loc <= 2) ? 2 : 1;
			size_t offset = loc == 1 ? 0u : loc == 2 ? 2u : (size_t)(loc + 1u);
			glVertexAttribPointer(loc, count, GL_FLOAT, GL_FALSE, sizeof(Instance),
			                      (const void *)(offset * sizeof(float)));
			glEnableVertexAttribArray(loc);
			glVertexAttribDivisor(loc, 1);
		}
		glBindVertexArray(0);
		initGlError = glGetError();
		if (initGlError != GL_NO_ERROR)
		{
			error = "GL error during diagnostic initialisation";
			return false;
		}
		ready = true;
		return true;
	}

	void drawOne(bool rgba, int sourceCol, int sourceRow, int panelCol, int panelRow,
	             float shade, float iso)
	{
		const int sw = screen->getWidth(), sh = screen->getHeight();
		const float margin = 12.0f, gap = 8.0f;
		const float panelW = (sw - margin * 2.0f - gap * 3.0f) / 4.0f;
		const float panelH = (sh - margin * 2.0f - gap * 2.0f) / 3.0f;
		const float scale = std::min((panelW - 10.0f) / cellW, (panelH - 10.0f) / cellH);
		const float drawW = cellW * scale, drawH = cellH * scale;
		const float x = margin + panelCol * (panelW + gap) + (panelW - drawW) * 0.5f;
		const float y = margin + panelRow * (panelH + gap) + (panelH - drawH) * 0.5f;
		Instance inst = {x, y, sourceCol / 4.0f, sourceRow / 3.0f,
		                 shade, 1.0f, 1.0f, iso};

		Shader &shader = rgba ? rgbaShader : r8Shader;
		shader.use();
		shader.setUniform2f("u_screenSize", (float)sw, (float)sh);
		shader.setUniform2f("u_tilePixelSize", drawW, drawH);
		shader.setUniform2f("u_tileUVSize", 0.25f, 1.0f / 3.0f);
		shader.setUniform1f("u_animFrame", 0.0f);
		shader.setUniform1i("u_atlas", 0);
		if (rgba)
		{
			rgbaAtlas.bind(0);
			shader.setUniform1i("u_shadeCurve", 3);
			shadeCurve.bind(3);
			shader.setUniform1i("u_hasNormalMap", 0);
			shader.setUniform1i("u_hasEmissive", 0);
			shader.setUniform1f("u_emissiveStrength", 0.0f);
		}
		else
		{
			r8Atlas.bind(0);
			shader.setUniform1i("u_shadeTable", 1);
			shadeTable.bind(1);
			shader.setUniform1f("u_unitShade", 0.0f);
		}
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(inst), &inst);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
	}

	void writeMetrics(bool ok)
	{
		const size_t raw = (size_t)atlasW * atlasH * 4u;
		const size_t chain = atlasW > 0 ? mipChainBytes(atlasW, atlasH, 4) : 0u;
		std::ofstream f(metricsPath.c_str(), std::ios::binary | std::ios::trunc);
		if (!f) return;
		f << "{\n"
		  << "  \"ok\": " << (ok ? "true" : "false") << ",\n"
		  << "  \"assetPath\": \"" << jsonEscape(assetPath) << "\",\n"
		  << "  \"atlas\": {\"width\": " << atlasW << ", \"height\": " << atlasH
		  << ", \"cellWidth\": " << cellW << ", \"cellHeight\": " << cellH
		  << ", \"baseRawBytes\": " << raw << ", \"mipChainBytes\": " << chain
		  << ", \"mipOverheadBytes\": " << (chain >= raw ? chain - raw : 0)
		  << ", \"computedGpuBytes\": " << chain
		  << ", \"cachedMirrorBytes\": " << rgbaAtlas.debugCachedBytes() << "},\n"
		  << "  \"reload\": {\"evicted\": " << (evicted ? "true" : "false")
		  << ", \"reuploaded\": " << (reuploaded ? "true" : "false") << "},\n"
		  << "  \"heap\": {\"beforeUpload\": " << (long long)heapBefore
		  << ", \"afterUpload\": " << (long long)heapAfterUpload
		  << ", \"afterReupload\": " << (long long)heapAfterReupload
		  << ", \"decodePeak\": " << (long long)heapDecodePeak
		  << ", \"reloadDecodePeak\": " << (long long)heapReloadPeak
		  << ", \"peakWasmUsed\": " << (long long)heapPeak
		  << ", \"peakDelta\": " << (long long)(heapPeak - heapBefore) << "},\n"
		  << "  \"gl\": {\"maxTextureSize\": " << maxTextureSize
		  << ", \"initError\": " << (unsigned)initGlError
		  << ", \"renderError\": " << (unsigned)renderGlError << "},\n"
		  << "  \"diagnostics\": {\"panelCount\": 12, \"shadeRows\": [0, 8, 15],"
		     " \"columns\": [\"rgba-order\", \"r8-fallback\", \"rgba-foreground-occlusion\", \"mixed-sparse-fallback\"]},\n"
		  << "  \"error\": \"" << jsonEscape(error) << "\"\n"
		  << "}\n";
	}

	void renderFrame()
	{
		if (!ready || frames >= 3) return;
		GlSave saved; saved.save();
		glViewport(0, 0, screen->getWidth(), screen->getHeight());
		glEnable(GL_SCISSOR_TEST);
		glScissor(0, 0, screen->getWidth(), screen->getHeight());
		glClearColor(0.025f, 0.035f, 0.055f, 1.0f);
		glDepthMask(GL_TRUE);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);

		const float shades[3] = {0.0f, 8.0f, 15.0f};
		for (int row = 0; row < 3; ++row)
		{
			const float shade = shades[row];
			/* RGBA: body -> right hand -> left hand. */
			drawOne(true, 0, row, 0, row, shade, 0.30f);
			drawOne(true, 1, row, 0, row, shade, 0.50f);
			drawOne(true, 2, row, 0, row, shade, 0.60f);
			/* Procedural palette fallback through tile_atlas + shade table. */
			drawOne(false, 0, row, 1, row, shade, 0.30f);
			/* Distinct hand order plus a foreground wall/sentinel on top. */
			drawOne(true, 0, row, 2, row, shade, 0.30f);
			drawOne(true, 2, row, 2, row, shade, 0.50f);
			drawOne(true, 1, row, 2, row, shade, 0.60f);
			drawOne(true, 3, row, 2, row, shade, 0.90f);
			/* Sparse/mixed: vanilla R8 body, RGBA hands and sentinel patches. */
			drawOne(false, 0, row, 3, row, shade, 0.30f);
			drawOne(true, 1, row, 3, row, shade, 0.50f);
			drawOne(true, 2, row, 3, row, shade, 0.60f);
			drawOne(true, 3, row, 3, row, shade, 0.70f);
		}
		ShaderManager::instance().setHadGPUPass(true);
		GLenum frameError = glGetError();
		if (renderGlError == GL_NO_ERROR && frameError != GL_NO_ERROR)
			renderGlError = frameError;
		saved.restore();

		++frames;
		if (frames == 3)
		{
			screen->screenshotGPU(outPath);
			if (renderGlError != GL_NO_ERROR) error = "GL error during diagnostic render";
			writeMetrics(renderGlError == GL_NO_ERROR);
			Log(renderGlError == GL_NO_ERROR ? LOG_INFO : LOG_ERROR)
				<< "HdUnitSpike: screenshot=" << outPath << " metrics=" << metricsPath;
		}
	}
};

} // namespace

bool HdUnitSpikeState::activate(Screen *screen, const std::string &assetPath,
	                            const std::string &outPath,
	                            const std::string &metricsPath)
{
	if (!screen || !GpuInit::ready())
	{
		Log(LOG_WARNING) << "HdUnitSpike: GPU pipeline not ready";
		return false;
	}
	auto pass = std::make_shared<HdUnitPass>(screen, assetPath, outPath, metricsPath);
	if (!pass->init())
	{
		pass->writeMetrics(false);
		Log(LOG_ERROR) << "HdUnitSpike: " << pass->error;
		return false;
	}
	screen->registerGPUPass([pass]() { pass->renderFrame(); });
	Log(LOG_INFO) << "HdUnitSpike: registered synthetic atlas " << assetPath;
	return true;
}

} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
