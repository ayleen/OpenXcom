#pragma once

#include <algorithm>
#include <cmath>

namespace OpenXcom
{
namespace Calypso
{

constexpr double GEOSCAPE_PI = 3.141592653589793238462643383279502884;
constexpr double GEOSCAPE_TWO_PI = GEOSCAPE_PI * 2.0;

struct GeoscapeLonLat
{
	double lon = 0.0;
	double lat = 0.0;
	bool operator==(const GeoscapeLonLat& other) const { return lon == other.lon && lat == other.lat; }
	bool operator!=(const GeoscapeLonLat& other) const { return !(*this == other); }
};

struct GeoscapeScreenPoint
{
	double x = 0.0;
	double y = 0.0;
};

struct GeoscapeProjection
{
	GeoscapeScreenPoint center;
	double radius = 0.0;
	double cameraLon = 0.0;
	double cameraLat = 0.0;
};

struct GeoscapeProjectedPoint
{
	GeoscapeScreenPoint point;
	bool visible = false;
};

struct GeoscapeUnprojectedPoint
{
	GeoscapeLonLat point;
	bool valid = false;
};

struct GeoscapeVec3
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

inline double calypsoGeoscapeNormalizeLon(double lon)
{
	lon = std::fmod(lon, GEOSCAPE_TWO_PI);
	if (lon < 0.0) lon += GEOSCAPE_TWO_PI;
	if (lon >= GEOSCAPE_TWO_PI) lon -= GEOSCAPE_TWO_PI;
	return lon;
}

inline GeoscapeVec3 calypsoGeoscapeToVec(GeoscapeLonLat p)
{
	const double cosLat = std::cos(p.lat);
	return {cosLat * std::sin(p.lon), std::sin(p.lat), cosLat * std::cos(p.lon)};
}

inline GeoscapeLonLat calypsoGeoscapeFromVec(GeoscapeVec3 v)
{
	const double length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	if (length > 0.0)
	{
		v.x /= length;
		v.y /= length;
		v.z /= length;
	}
	return {calypsoGeoscapeNormalizeLon(std::atan2(v.x, v.z)),
		std::asin(std::max(-1.0, std::min(1.0, v.y)))};
}

inline GeoscapeVec3 calypsoGeoscapeRotateX(GeoscapeVec3 v, double a)
{
	const double s = std::sin(a), c = std::cos(a);
	return {v.x, c * v.y - s * v.z, s * v.y + c * v.z};
}

inline GeoscapeVec3 calypsoGeoscapeRotateY(GeoscapeVec3 v, double a)
{
	const double s = std::sin(a), c = std::cos(a);
	return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}

inline GeoscapeVec3 calypsoGeoscapeRotateZ(GeoscapeVec3 v, double a)
{
	const double s = std::sin(a), c = std::cos(a);
	return {c * v.x - s * v.y, s * v.x + c * v.y, v.z};
}

class GeoscapeRegistration
{
public:
	GeoscapeRegistration(double yaw, double pitch, double roll)
		: _yaw(yaw), _pitch(pitch), _roll(roll) {}

	GeoscapeLonLat forward(GeoscapeLonLat p) const
	{
		if (identity()) return p;
		auto v = calypsoGeoscapeToVec(p);
		v = calypsoGeoscapeRotateY(v, _yaw);
		v = calypsoGeoscapeRotateX(v, _pitch);
		v = calypsoGeoscapeRotateZ(v, _roll);
		return calypsoGeoscapeFromVec(v);
	}

	GeoscapeLonLat inverse(GeoscapeLonLat p) const
	{
		if (identity()) return p;
		auto v = calypsoGeoscapeToVec(p);
		v = calypsoGeoscapeRotateZ(v, -_roll);
		v = calypsoGeoscapeRotateX(v, -_pitch);
		v = calypsoGeoscapeRotateY(v, -_yaw);
		return calypsoGeoscapeFromVec(v);
	}

	bool identity() const { return _yaw == 0.0 && _pitch == 0.0 && _roll == 0.0; }

private:
	double _yaw;
	double _pitch;
	double _roll;
};

inline GeoscapeRegistration calypsoGeoscapeRegistration(double yaw, double pitch, double roll)
{
	return GeoscapeRegistration(yaw, pitch, roll);
}

inline GeoscapeProjectedPoint calypsoGeoscapeProject(const GeoscapeProjection& p, GeoscapeLonLat point)
{
	const double delta = point.lon - p.cameraLon;
	const double cosLat = std::cos(point.lat);
	const double visibility = std::cos(p.cameraLat) * cosLat * std::cos(delta)
		+ std::sin(p.cameraLat) * std::sin(point.lat);
	return {{
		p.center.x + p.radius * cosLat * std::sin(delta),
		p.center.y + p.radius * (std::cos(p.cameraLat) * std::sin(point.lat)
			- std::sin(p.cameraLat) * cosLat * std::cos(delta))},
		p.radius > 0.0 && visibility >= 0.0};
}

inline GeoscapeUnprojectedPoint calypsoGeoscapeUnproject(const GeoscapeProjection& p, GeoscapeScreenPoint point)
{
	if (p.radius <= 0.0) return {};
	const double x = point.x - p.center.x;
	const double y = point.y - p.center.y;
	const double rho = std::sqrt(x * x + y * y);
	if (rho > p.radius) return {};
	if (rho == 0.0) return {{calypsoGeoscapeNormalizeLon(p.cameraLon), p.cameraLat}, true};
	const double c = std::asin(std::min(1.0, rho / p.radius));
	const double sinC = std::sin(c), cosC = std::cos(c);
	const double lat = std::asin((y * sinC * std::cos(p.cameraLat)) / rho
		+ cosC * std::sin(p.cameraLat));
	const double lon = std::atan2(x * sinC,
		rho * std::cos(p.cameraLat) * cosC - y * std::sin(p.cameraLat) * sinC)
		+ p.cameraLon;
	return {{calypsoGeoscapeNormalizeLon(lon), lat}, true};
}

inline double calypsoGeoscapeAngularDistance(GeoscapeLonLat a, GeoscapeLonLat b)
{
	const auto av = calypsoGeoscapeToVec(a);
	const auto bv = calypsoGeoscapeToVec(b);
	const double dot = std::max(-1.0, std::min(1.0, av.x * bv.x + av.y * bv.y + av.z * bv.z));
	const double crossX = av.y * bv.z - av.z * bv.y;
	const double crossY = av.z * bv.x - av.x * bv.z;
	const double crossZ = av.x * bv.y - av.y * bv.x;
	const double cross = std::sqrt(crossX * crossX + crossY * crossY + crossZ * crossZ);
	return std::atan2(cross, dot);
}

} // namespace Calypso
} // namespace OpenXcom
