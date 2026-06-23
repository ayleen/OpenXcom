/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "TTFUtil.h"
#include "Surface.h"

namespace OpenXcom
{
namespace TTFUtil
{

void blitFit(SDL_Surface* ttf, Surface* destS, HAlign halign, VAlign valign, float fillFrac)
{
	if (!ttf || !destS || !destS->getSurface()) return;
	SDL_Surface* dst = destS->getSurface();
	if (ttf->format->BitsPerPixel != 32 || dst->format->BitsPerPixel != 32) return;
	destS->clear();
	const int dW = dst->w, dH = dst->h, sW = ttf->w, sH = ttf->h;
	if (sW <= 0 || sH <= 0 || dW <= 0 || dH <= 0) return;

	// Fit (downscale only), then shrink by fillFrac.
	const float scaleH = (float)dH / (float)sH;
	const float scaleW = (float)dW / (float)sW;
	float scale = scaleH < scaleW ? scaleH : scaleW;
	if (scale > 1.0f) scale = 1.0f;
	scale *= fillFrac;
	int outW = (int)(sW * scale + 0.5f); if (outW < 1) outW = 1; if (outW > dW) outW = dW;
	int outH = (int)(sH * scale + 0.5f); if (outH < 1) outH = 1; if (outH > dH) outH = dH;

	int ox = 0, oy = 0;
	switch (halign) { case H_LEFT: ox = 0; break; case H_CENTER: ox = (dW - outW) / 2; break; case H_RIGHT: ox = dW - outW; break; }
	switch (valign) { case V_TOP: oy = 0; break; case V_MIDDLE: oy = (dH - outH) / 2; break; case V_BOTTOM: oy = dH - outH; break; }
	if (ox < 0) ox = 0;
	if (oy < 0) oy = 0;

	SDL_LockSurface(ttf); SDL_LockSurface(dst);
	for (int y = 0; y < outH; ++y)
	{
		float sy = (y + 0.5f) * sH / outH - 0.5f; if (sy < 0) sy = 0; if (sy > sH - 1) sy = sH - 1;
		const int y0 = (int)sy; const int y1 = (y0 + 1 < sH) ? y0 + 1 : y0; const float fy = sy - y0;
		Uint32* drow = (Uint32*)((Uint8*)dst->pixels + (oy + y) * dst->pitch);
		Uint32* r0 = (Uint32*)((Uint8*)ttf->pixels + y0 * ttf->pitch);
		Uint32* r1 = (Uint32*)((Uint8*)ttf->pixels + y1 * ttf->pitch);
		for (int x = 0; x < outW; ++x)
		{
			float sx = (x + 0.5f) * sW / outW - 0.5f; if (sx < 0) sx = 0; if (sx > sW - 1) sx = sW - 1;
			const int x0 = (int)sx; const int x1 = (x0 + 1 < sW) ? x0 + 1 : x0; const float fx = sx - x0;
			Uint8 ar,ag,ab,aa, br,bg,bb,ba, cr,cg,cb,ca, er,eg,eb,ea;
			SDL_GetRGBA(r0[x0], ttf->format, &ar,&ag,&ab,&aa);
			SDL_GetRGBA(r0[x1], ttf->format, &br,&bg,&bb,&ba);
			SDL_GetRGBA(r1[x0], ttf->format, &cr,&cg,&cb,&ca);
			SDL_GetRGBA(r1[x1], ttf->format, &er,&eg,&eb,&ea);
			const float w00=(1-fx)*(1-fy), w10=fx*(1-fy), w01=(1-fx)*fy, w11=fx*fy;
			const Uint8 R=(Uint8)(ar*w00+br*w10+cr*w01+er*w11+0.5f);
			const Uint8 G=(Uint8)(ag*w00+bg*w10+cg*w01+eg*w11+0.5f);
			const Uint8 B=(Uint8)(ab*w00+bb*w10+cb*w01+eb*w11+0.5f);
			const Uint8 A=(Uint8)(aa*w00+ba*w10+ca*w01+ea*w11+0.5f);
			drow[ox + x] = SDL_MapRGBA(dst->format, R, G, B, A);
		}
	}
	SDL_UnlockSurface(ttf); SDL_UnlockSurface(dst);
}

void blitStretch(SDL_Surface* src, Surface* destS)
{
	if (!src || !destS || !destS->getSurface()) return;
	SDL_Surface* dst = destS->getSurface();
	if (src->format->BitsPerPixel != 32 || dst->format->BitsPerPixel != 32) return;
	const int dW = dst->w, dH = dst->h, sW = src->w, sH = src->h;
	if (sW <= 0 || sH <= 0 || dW <= 0 || dH <= 0) return;

	SDL_LockSurface(src); SDL_LockSurface(dst);
	for (int y = 0; y < dH; ++y)
	{
		float sy = (y + 0.5f) * sH / dH - 0.5f; if (sy < 0) sy = 0; if (sy > sH - 1) sy = sH - 1;
		const int y0 = (int)sy; const int y1 = (y0 + 1 < sH) ? y0 + 1 : y0; const float fy = sy - y0;
		Uint32* drow = (Uint32*)((Uint8*)dst->pixels + y * dst->pitch);
		Uint32* r0 = (Uint32*)((Uint8*)src->pixels + y0 * src->pitch);
		Uint32* r1 = (Uint32*)((Uint8*)src->pixels + y1 * src->pitch);
		for (int x = 0; x < dW; ++x)
		{
			float sx = (x + 0.5f) * sW / dW - 0.5f; if (sx < 0) sx = 0; if (sx > sW - 1) sx = sW - 1;
			const int x0 = (int)sx; const int x1 = (x0 + 1 < sW) ? x0 + 1 : x0; const float fx = sx - x0;
			Uint8 ar,ag,ab,aa, br,bg,bb,ba, cr,cg,cb,ca, er,eg,eb,ea;
			SDL_GetRGBA(r0[x0], src->format, &ar,&ag,&ab,&aa);
			SDL_GetRGBA(r0[x1], src->format, &br,&bg,&bb,&ba);
			SDL_GetRGBA(r1[x0], src->format, &cr,&cg,&cb,&ca);
			SDL_GetRGBA(r1[x1], src->format, &er,&eg,&eb,&ea);
			const float w00=(1-fx)*(1-fy), w10=fx*(1-fy), w01=(1-fx)*fy, w11=fx*fy;
			const Uint8 R=(Uint8)(ar*w00+br*w10+cr*w01+er*w11+0.5f);
			const Uint8 G=(Uint8)(ag*w00+bg*w10+cg*w01+eg*w11+0.5f);
			const Uint8 B=(Uint8)(ab*w00+bb*w10+cb*w01+eb*w11+0.5f);
			const Uint8 A=(Uint8)(aa*w00+ba*w10+ca*w01+ea*w11+0.5f);
			drow[x] = SDL_MapRGBA(dst->format, R, G, B, A);
		}
	}
	SDL_UnlockSurface(src); SDL_UnlockSurface(dst);
}

}
}
