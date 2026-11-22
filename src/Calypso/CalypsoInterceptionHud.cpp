#ifdef __EMSCRIPTEN__
#include "CalypsoInterceptionHud.h"
#include "CalypsoF21UiShared.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoHdImageSource.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Geoscape/DogfightState.h"
#include "../Engine/Game.h"
#include "../Engine/TTFFont.h"
#include "../Engine/Unicode.h"
#include <numeric>
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/ImageButton.h"
#include "../Interface/Text.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Ufo.h"
#include "../Savegame/CraftWeapon.h"
#include "../Savegame/CraftWeaponProjectile.h"
#include "../Mod/RuleCraftWeapon.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/Mod.h"
#include <cmath>

namespace OpenXcom { namespace Calypso {
namespace {
bool contains(const CalypsoLogicalRect &r, int x, int y)
{
	return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

void decodeArt(CalypsoHdImageDescriptor &art, const std::string &key, Surface *surface, int top, int height)
{
	if (art.pixels) return;
	auto image = Surface::NewSdlSurface(SDL_ConvertSurfaceFormat(surface->getSurface(), SDL_PIXELFORMAT_RGBA32, 0));
	if (!image || top < 0 || height < 1 || top + height > image->h)
		CalypsoHdUiOverlay::instance().failHdRoute("Could not decode interception image");
	auto pixels = std::make_shared<CalypsoHdImageRgba>();
	pixels->w = image->w;
	pixels->h = height;
	for (int row = top; row < top + height; ++row)
	{
		const auto *start = static_cast<const Uint8*>(image->pixels) + row * image->pitch;
		pixels->px.insert(pixels->px.end(), start, start + image->w * 4);
	}
	art.source = key;
	art.pixels = pixels;
}

const CalypsoHdImageDescriptor &sonarRing()
{
	static const auto ring = [] {
		CalypsoHdImageDescriptor art;
		auto pixels = std::make_shared<CalypsoHdImageRgba>();
		pixels->w = pixels->h = 192;
		pixels->px.resize(192 * 192 * 4);
		for (int y = 0; y < 192; ++y) for (int x = 0; x < 192; ++x)
		{
			const double distance = std::hypot(x - 95.5, y - 95.5);
			auto *pixel = &pixels->px[4 * (y * 192 + x)];
			pixel[0] = 98; pixel[1] = 172; pixel[2] = 150;
			pixel[3] = Uint8(200 * std::max(0.0, 1.0 - std::abs(distance - 94.8)));
		}
		art.source = "calypso-interception-ring";
		art.pixels = pixels;
		return art;
	}();
	return ring;
}
}

void CalypsoInterceptionHud::collect(GeoscapeState &geo, CalypsoHdFrameBuilder &builder)
{
	if (geo._dogfights.empty()) return;
	auto *game = getCurrentGame();
	CalypsoTtfSourceDescriptor font;
	if (!calypsoHdResolveFontDescriptor(game->getMod(), "FONT_F33_BODY", font))
		CalypsoHdUiOverlay::instance().failHdRoute("Missing interception font");
	const auto &metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	const int width = Options::baseXResolution, height = Options::baseYResolution;
	const int expanded = std::count_if(geo._dogfights.begin(), geo._dogfights.end(), [](DogfightState *f){return !f->_minimized;});
	const int columns = expanded > 1 ? 2 : 1, rows = expanded > 2 ? 2 : 1;
	const int dockCount = int(geo._dogfights.size()) - expanded;
	const int dockH = std::max(10, height / 14);
	const int area = height - (dockCount ? dockH + 2 : 0);
	int index = 0, dockIndex = 0;
	for (auto *f : geo._dogfights)
	{
		auto &v = f->_hdView;
		v.controls.clear();
		if (f->_minimized)
		{
			const int step = width / dockCount;
			v.card = {step * dockIndex++, height - dockH, step - 1, dockH};
		}
		else
		{
			const int cellW = width / columns, cellH = area / rows;
			const int w = cellW * (columns == 1 ? 80 : 97) / 100, h = cellH * (rows == 1 ? 78 : 96) / 100;
			v.card = {cellW * (index % columns) + (cellW-w)/2, cellH * (index / columns) + (cellH-h)/2, w, h};
			++index;
		}
		const auto &card = v.card;
		builder.beginSubgroup();
		CalypsoF21Painter painter{builder, 0xf019u, f->_displayId, 0, 1.0f, 1.0, {}, metrics.scaleX, metrics.scaleY};
		uint32_t role = 1;
		auto rect = [&](int x,int y,int w,int h) {
			return CalypsoLogicalRect{card.x + x*card.w/1000, card.y + y*card.h/500,
				std::max(1,w*card.w/1000), std::max(1,h*card.h/500)};
		};
		auto label = [&](CalypsoLogicalRect box, const std::string &text, double size, bool center = false) {
			painter.textRect(box, nullptr, font, text, 0xe1f5efffu,
				center ? CalypsoHdHAlign::Center : CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
				1, role++, 0.0, size, false);
		};
		const double body = card.h * 0.045;
		auto control = [&](CalypsoLogicalRect box, const std::string &text,int cmd,int slot,bool selected=false) {
			auto &width = v.labelWidths[cmd * 64 + slot];
			if (width.first != text)
			{
				auto *face = game->getMod()->getTTFFont("FONT_F33_BODY", false);
				std::vector<int> advances, kerning;
				if (!face || !face->measureGlyphs(Unicode::convUtf8ToUtf32(text), advances, kerning))
					CalypsoHdUiOverlay::instance().failHdRoute("Could not measure interception label");
				const int pixels = std::accumulate(advances.begin(), advances.end(), 0)
					+ std::accumulate(kerning.begin(), kerning.end(), 0);
				width = {text, double(pixels) / face->pixelSize()};
			}
			bool focused = v.focus == int(v.controls.size());
			painter.decoration(box, focused ? 0xbca467ffu : 0x447273ffu, role++);
			painter.decoration({box.x+1,box.y+1,box.w-2,box.h-2}, selected ? 0x25645affu : 0x102e33ffu, role++);
			const double height = cmd==7 ? box.h*0.55 : std::min(body,box.h*0.65);
			const double fit = (box.w - 2) * 0.90 / std::max(1.0, width.second);
			label(box,text,std::min(height, fit),true);
			v.controls.push_back({box,cmd,slot});
		};
		const auto craft = f->_craft->getName(game->getLanguage());
		const auto target = f->_ufo->getName(game->getLanguage());
		if (f->_minimized) { control(card,craft+" · "+target,7,0); continue; }
		painter.decoration(card,0x508c83ffu,role++);
		painter.decoration({card.x+1,card.y+1,card.w-2,card.h-2},0x081d22ffu,role++);
		label(rect(28,8,600,49),craft,body*1.38);
		label(rect(615,8,360,49),f->_txtStatus->getText(),body);
		const bool preview = f->_preview->getVisible();
		const auto radar = rect(27,68,450,294);
		painter.decoration(radar,0x041518ffu,role++);
		const int cx = radar.x + radar.w/2, cy = radar.y + radar.h/2;
		if (preview)
		{
			const auto *skin = game->getMod()->getInterface("dogfight");
			const int top = skin->getElement("previewTop")->h;
			decodeArt(v.target,"calypso-interception-target-"+std::to_string(f->_displayId),f->_preview,top,
				f->_preview->getHeight()-top-skin->getElement("previewBot")->h);
			const double fit = std::min(double(radar.w)/v.target.pixels->w, double(radar.h)/v.target.pixels->h);
			const int w = std::max(1,int(v.target.pixels->w*fit)), h = std::max(1,int(v.target.pixels->h*fit));
			painter.image({cx-w/2,cy-h/2,w,h},v.target,nullptr,role++);
		}
		else
		{
			const int radius = std::min(radar.h/2-2, int(radar.w*metrics.scaleX/metrics.scaleY/2)-2);
			const double aspect = metrics.scaleY/metrics.scaleX;
			auto ring = [&](int r) {
				if (r < 1) return;
				const int rx = std::max(1,int(r*aspect));
				painter.image({cx-rx,cy-r,rx*2,r*2},sonarRing(),nullptr,role++);
			};
			int limit = std::max(640, f->_currentDist);
			for (auto *weapon : *f->_craft->getWeapons()) if (weapon) limit = std::max(limit, weapon->getRules()->getRange()*8);
			for (int i=1;i<=4;++i) ring(radius*i/4);
			painter.decoration({radar.x,cy,radar.w,1},0x264745ffu,role++);
			painter.decoration({cx,radar.y,1,radar.h},0x264745ffu,role++);
			for (int i=0;i<f->_weaponNum;++i)
			{
				auto *weapon=f->_craft->getWeapons()->at(i);
				if (weapon && f->_weaponEnabled[i]) ring(radius*weapon->getRules()->getRange()*8/limit);
			}
			const auto yAt = [&](int position){return cy - radius * std::max(0,std::min(limit,position))/limit;};
			const int ty=yAt(f->_currentDist), dot=std::max(1,(f->_ufoSize+2)*card.h/260);
			painter.decoration({cx-dot,ty-dot,dot*2,dot*2},f->_animatingHit?0xffffb0ffu:0xfa9856ffu,role++);
			for (auto *shot : f->_projectiles)
			{
				const int y = yAt(shot->getPosition());
				const bool outgoing = shot->getDirection()==D_UP;
				const int end = shot->getGlobalType()==CWPGT_BEAM ? (outgoing?ty:cy) : y+(outgoing?2:-2);
				painter.decoration({cx+shot->getHorizontalPosition(),std::min(y,end),1,std::max(1,std::abs(end-y))},
					outgoing?0xb3ffbdffu:0xff9977ffu,role++);
			}
			decodeArt(v.ship,"calypso-interception-ship-"+std::to_string(f->_displayId),f->_craftSprite,0,f->_craftSprite->getHeight());
			const int size=std::max(3,radar.h/5);
			painter.image({cx-size/2,cy-size/2,size,size},v.ship,nullptr,role++);
		}
		label(rect(505,68,460,45),target,body*1.18);
		label(rect(505,114,460,38),f->_txtDistance->getText()+"  "+f->_txtOceanIndicator->getText(),body);
		std::string health = std::string(f->tr("STR_CAL_INTERCEPT_HULL"))+" "+std::to_string(100-f->_craft->getDamagePercentage())+"%";
		if (f->_craft->getShieldCapacity()) health += "   "+std::string(f->tr("STR_CAL_INTERCEPT_SHIELD"))+" "+std::to_string(f->_craft->getShieldPercentage())+"%";
		label(rect(505,153,480,39),health,body);
		const char *modes[]={"STR_STANDOFF","STR_CAUTIOUS_ATTACK","STR_STANDARD_ATTACK","STR_AGGRESSIVE_ATTACK","STR_DISENGAGING"};
		ImageButton *buttons[]={f->_btnStandoff,f->_btnCautious,f->_btnStandard,f->_btnAggressive,f->_btnDisengage};
		int mode=-1;
		if (!preview)
		{
			const int cell=150/std::max(1,f->_weaponNum);
			for (int i=0;i<f->_weaponNum;++i)
			{
				auto *weapon=f->_craft->getWeapons()->at(i);
				if (weapon && f->_weapon[i]->getVisible()) control(rect(505,198+i*cell,465,std::min(50,cell)-2),
					std::string(f->tr(weapon->getRules()->getType()))+" · "+std::to_string(weapon->getAmmo())+" · "+std::string(f->tr(f->_weaponEnabled[i] ? "STR_CAL_INTERCEPT_WEAPON_ON" : "STR_CAL_INTERCEPT_WEAPON_OFF")),8,i,f->_weaponEnabled[i]);
			}
			for (int i=0;i<5;++i)
			{
				if (buttons[i]==f->_mode) mode=i;
				if (buttons[i]->getVisible()) control(rect(26+i*190,372,185,50),f->tr(modes[i]),i,0,buttons[i]==f->_mode);
			}
		}
		if (f->_btnUfo->getVisible() || preview)
			control(rect(26,435,285,49),f->tr(preview?"STR_CAL_INTERCEPT_BACK":"STR_CAL_INTERCEPT_INSPECT"),6,0);
		if (!preview && expanded>1 && mode>=0 && !v.confirming)
			control(rect(323,435,279,49),f->tr("STR_CAL_INTERCEPT_ALL"),9,mode);
		if (!preview && f->_btnMinimize->getVisible())
		{
			const char *key = "STR_CAL_INTERCEPT_MINIMIZE";
			if (f->_craftIsDefenseless)
				key = f->_selfDestructPressed?"STR_CAL_INTERCEPT_CANCEL_DESTRUCT":v.confirming?"STR_CAL_INTERCEPT_CONFIRM":"STR_CAL_INTERCEPT_DESTRUCT";
			control(rect(614,435,357,49),f->tr(key),5,0);
			if (v.confirming) control(rect(323,435,279,49),f->tr("STR_CAL_INTERCEPT_BACK"),10,0);
		}
	}
}

bool CalypsoInterceptionHud::handle(GeoscapeState &geo, Action *action)
{
	if (!action || geo._dogfights.empty()) return false;
	auto *event = action->getDetails();
	for (auto *f : geo._dogfights)
	{
		auto &v=f->_hdView;
		if (v.controls.empty()) continue;
		int index=-1;
		if (event->type==SDL_MOUSEBUTTONDOWN || event->type==SDL_MOUSEBUTTONUP)
		{
			action->setMouseAction(event->button.x,event->button.y,0,0);
			const int x=action->getAbsoluteXMouse(), y=action->getAbsoluteYMouse();
			if (!contains(v.card,x,y)) continue;
			if (event->type==SDL_MOUSEBUTTONDOWN || event->button.button!=SDL_BUTTON_LEFT) return true;
			for (size_t i=0;i<v.controls.size();++i) if (contains(v.controls[i].rect,x,y)) {index=int(i); break;}
			if (index<0) return true;
		}
		else if (event->type==SDL_KEYDOWN)
		{
			const auto key=event->key.keysym.sym;
			if (key==SDLK_TAB)
			{
				int direction = (event->key.keysym.mod & KMOD_SHIFT) ? -1 : 1;
				v.focus=(v.focus+direction+int(v.controls.size()))%int(v.controls.size());
				return true;
			}
			if ((key!=SDLK_RETURN && key!=SDLK_SPACE) || v.focus<0 || v.focus>=int(v.controls.size())) continue;
			index=v.focus;
		}
		else return false;
		const auto selected=v.controls[index];
		if (selected.command==10) v.confirming=false;
		else if (selected.command==5 && f->_craftIsDefenseless && !f->_selfDestructPressed && !v.confirming)
			v.confirming=true;
		else
		{
			v.confirming=false;
			f->interceptionAction(selected.command,selected.slot);
			v.focus=-1;
			v.controls.clear();
		}
		return true;
	}
	return false;
}

} }
#endif
