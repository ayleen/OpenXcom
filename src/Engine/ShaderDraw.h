#pragma once
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
#include "ShaderDrawHelper.h"  // includes Surface.h transitively
#include "ShadeTable.h"
#include <tuple>

namespace OpenXcom
{

template<typename First, typename... Rest>
static inline First&& GetFirst(First&& f, Rest&&... r)
{
	return std::forward<First>(f);
}

/**
 * Universal blit function implementation.
 * @param f called function.
 * @param src source surfaces control objects.
 */
template<typename Func, typename... SrcType>
static inline void ShaderDrawImpl(Func&& f, helper::controler<SrcType>... src)
{
	//get basic draw range in 2d space
	GraphSubset end_temp = GetFirst(src...).get_range();

	//intersections with src ranges
	(src.mod_range(end_temp), ...);

	const GraphSubset end = end_temp;
	if (!end)
		return;

	//set final draw range in 2d space
	(src.set_range(end), ...);


	int begin_y = 0, end_y = end.size_y();

	//determining iteration range in y-axis
	(src.mod_y(begin_y, end_y), ...);

	if(begin_y>=end_y)
		return;

	//set final iteration range
	(src.set_y(begin_y, end_y), ...);

	//iteration on y-axis
	for (int y = end_y-begin_y; y>0; --y, (src.inc_y(), ...))
	{
		int begin_x = 0, end_x = end.size_x();

		//determining iteration range in x-axis
		(src.mod_x(begin_x, end_x), ...);

		if (begin_x>=end_x)
			continue;

		//set final iteration range
		(src.set_x(begin_x, end_x), ...);

		int size_x = end_x-begin_x;
		//iteration on x-axis
		for (int x = size_x / 4; x>0; --x)
		{
			f(src.get_ref()...); (src.inc_x(), ...);
			f(src.get_ref()...); (src.inc_x(), ...);
			f(src.get_ref()...); (src.inc_x(), ...);
			f(src.get_ref()...); (src.inc_x(), ...);
		}
		if (size_x & 2)
		{
			f(src.get_ref()...); (src.inc_x(), ...);
			f(src.get_ref()...); (src.inc_x(), ...);
		}
		if (size_x & 1)
		{
			f(src.get_ref()...); (src.inc_x(), ...);
		}
	}

};

/**
 * Universal blit function.
 * @tparam ColorFunc class that contains static function `func`.
 * function is used to modify these arguments.
 * @param src_frame destination and source surfaces modified by function.
 */
template<typename ColorFunc, typename... SrcType>
static inline void ShaderDraw(const SrcType&... src_frame)
{
	ShaderDrawImpl([](auto&&... a){ ColorFunc::func(std::forward<decltype(a)>(a)...); }, helper::controler<SrcType>(src_frame)...);
}

/**
 * Universal blit function.
 * @param f function that modify other arguments.
 * @param src_frame destination and source surfaces modified by function.
 */
template<typename Func, typename... SrcType>
static inline void ShaderDrawFunc(Func&& f, const SrcType&... src_frame)
{
	ShaderDrawImpl(std::forward<Func>(f), helper::controler<SrcType>(src_frame)...);
}

namespace helper
{

const Uint8 ColorGroup = 0xF0;
const Uint8 ColorShade = 0x0F;

/**
 * help class used for Surface::blitNShade
 */
struct ColorReplace
{
	/// 7.B / R1.1: ARGB overload — srcIdx is the original palette index from _paletteMirror.
	static inline void func(Uint32& dest, const Uint32& src, const Uint8& srcIdx,
	                        const int& shade, const int& newBaseColor,
	                        const ShadeTable *table, const ShadeTable *recolouredTable)
	{
		if ((src >> 24) == 0) return;
		// 8a-fix: srcIdx==0 means no _paletteMirror (or transparent index) — fall
		// back to ARGB curve so non-transparent pixels don't collapse to table[0]==0.
		if (srcIdx == 0)
		{
			dest = ::OpenXcom::shadeARGBCurve(src, shade);
			return;
		}
		if (recolouredTable)
			dest = recolouredTable->get(srcIdx, shade);
		else if (table)
		{
			const Uint8 newIdx = (Uint8)((srcIdx & ColorShade) | (Uint8)newBaseColor);
			dest = table->get(newIdx, shade);
		}
		else
			dest = ::OpenXcom::shadeARGBCurve(src, shade);
	}
};

/**
 * help class used for Surface::blitNShade
 */
struct StandardShade
{
	/// 7.B / R1.1: ARGB overload — srcIdx is the original palette index from _paletteMirror.
	/// If table is null (HD asset), falls back to shadeARGBCurve.
	static inline void func(Uint32& dest, const Uint32& src, const Uint8& srcIdx,
	                        const int& shade, const ShadeTable *table)
	{
		if ((src >> 24) == 0) return;
		// 8a-fix: see ColorReplace — srcIdx==0 → ARGB curve, not table[0].
		if (!table || srcIdx == 0)
			dest = ::OpenXcom::shadeARGBCurve(src, shade);
		else
			dest = table->get(srcIdx, shade);
	}
};

/**
 * helper class used for blitting dying unit with overkill
 */
struct BurnShade
{
	/// 7.B / R1.1: ARGB overload — srcIdx is the original palette index from _paletteMirror.
	static inline void func(Uint32& dest, const Uint32& src, const Uint8& srcIdx,
	                        const int& burn, const int& shade,
	                        const ShadeTable *table)
	{
		if ((src >> 24) == 0) return;
		if (!table || srcIdx == 0)
		{
			dest = ::OpenXcom::shadeARGBCurve(src, shade);
			return;
		}
		if (burn)
		{
			const Uint8 tempBurn = (srcIdx & ColorShade) + (Uint8)burn;
			if (tempBurn > 26)
				dest = table->get(srcIdx, shade);
			else if (tempBurn > 15)
				dest = table->get(ColorShade, shade);
			else
			{
				const Uint8 burned = (Uint8)((srcIdx & ColorGroup) + tempBurn);
				dest = table->get(burned, shade);
			}
		}
		else
			dest = table->get(srcIdx, shade);
	}
};

}//namespace helper

template<typename T>
static inline helper::Scalar<T> ShaderScalar(T& t)
{
	return helper::Scalar<T>(t);
}
template<typename T>
static inline helper::Scalar<const T> ShaderScalar(const T& t)
{
	return helper::Scalar<const T>(t);
}

/**
 * Create warper from vector
 * @param s vector
 * @return
 */
template<typename Pixel>
inline helper::ShaderBase<Pixel> ShaderSurface(std::vector<Pixel>& s, int max_x, int max_y)
{
	return helper::ShaderBase<Pixel>(s, max_x, max_y);
}

/**
 * Create warper from array
 * @param s array
 * @return
 */
template<typename Pixel, int Size>
inline helper::ShaderBase<Pixel> ShaderSurface(Pixel(&s)[Size], int max_x, int max_y)
{
	return helper::ShaderBase<Pixel>(s, max_x, max_y, max_x*sizeof(Pixel));
}

}//namespace OpenXcom
