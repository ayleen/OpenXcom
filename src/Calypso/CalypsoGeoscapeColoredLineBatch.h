#pragma once
/*
 * Phase 46.4 SS15 P0 -- Geoscape radar/flight coloured-line batch contracts.
 *
 * Portable, engine-independent half of the one-draw radar/flight correction:
 * packed-vertex layout, bounded command recording, ordered packing,
 * allocation-free snapshot keys/generations, and New Base range
 * canonicalization. Native doctests exercise these contracts directly; the
 * Emscripten resource owner (CalypsoGeoscapeColoredLineBatch.cpp) consumes
 * the same declarations so production and tests cannot drift apart.
 *
 * Pure and engine-independent: no SDL, no GL, no allocations on reuse paths.
 */
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

/// Interleaved physical vertex: NDC position plus one normalized RGBA colour.
/// Attribute 0 = vec2 position at offset 0; attribute 1 = 4 x
/// GL_UNSIGNED_BYTE normalized colour at offset 8. Both vertices generated
/// from one raster-step command carry the identical resolved RGBA.
struct CalypsoGeoscapeColoredLineVertex
{
	float x;
	float y;
	std::uint8_t r;
	std::uint8_t g;
	std::uint8_t b;
	std::uint8_t a;
};

static_assert(sizeof(CalypsoGeoscapeColoredLineVertex) == 12,
	"coloured-line vertex must stay tightly packed at 12 bytes");
static_assert(alignof(CalypsoGeoscapeColoredLineVertex) == 4,
	"coloured-line vertex alignment must stay 4 bytes");

/// Byte offsets of the two vertex attributes; the VAO must use these.
static const size_t COLORED_LINE_POSITION_OFFSET = 0u;
static const size_t COLORED_LINE_COLOR_OFFSET = offsetof(CalypsoGeoscapeColoredLineVertex, r);

/// One resolved raster-step line command in logical globe-surface pixels
/// with its final effective-palette RGBA already applied.
struct CalypsoGeoscapeColoredLineCommand
{
	double x1;
	double y1;
	double x2;
	double y2;
	std::uint8_t r;
	std::uint8_t g;
	std::uint8_t b;
	std::uint8_t a;
};

/// Hard per-frame bound for the coloured-line batch (mirrors the historical
/// radar/flight command capacity; overflow fails closed before publication).
static const size_t COLORED_LINE_COMMAND_CAPACITY = 16384u;
static const size_t COLORED_LINE_VERTEX_CAPACITY =
	COLORED_LINE_COMMAND_CAPACITY * 2u;

/// Physical mapping inputs shared by every pack operation. Positions are
/// projected from the same frozen physical globe rectangle used by Earth,
/// markers, labels, and hit testing.
struct CalypsoGeoscapeColoredLineViewport
{
	double rectX;
	double rectY;
	double scaleX;
	double scaleY;
	double displayWidth;
	double displayHeight;
};

/// Ordered, bounds-checked CPU snapshot of the current radar/flight stream.
/// Recording refuses growth past the declared capacity; packing writes each
/// command as exactly two consecutive vertices in original command order.
class CalypsoGeoscapeColoredLineBatchState
{
public:
	CalypsoGeoscapeColoredLineBatchState()
	{
		_commands.reserve(COLORED_LINE_COMMAND_CAPACITY);
		_vertices.reserve(COLORED_LINE_VERTEX_CAPACITY);
	}

	void clearCommands()
	{
		_commands.clear();
		_vertices.clear();
	}

	/// Bounds-checked append. Returns false (recording nothing) when the
	/// batch is full; the caller fails the route before publication.
	bool tryRecordCommand(double x1, double y1, double x2, double y2,
		std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
	{
		if (_commands.size() >= COLORED_LINE_COMMAND_CAPACITY)
			return false;
		CalypsoGeoscapeColoredLineCommand command;
		command.x1 = x1; command.y1 = y1;
		command.x2 = x2; command.y2 = y2;
		command.r = r; command.g = g; command.b = b; command.a = a;
		_commands.push_back(command);
		return true;
	}

	size_t commandCount() const { return _commands.size(); }
	size_t vertexCount() const { return _commands.size() * 2u; }

	const CalypsoGeoscapeColoredLineCommand* commands() const
	{
		return _commands.empty() ? nullptr : &_commands[0];
	}

	/// Packs committed commands into the interleaved vector in original
	/// order using the frozen viewport. Returns the vertex count (x2), or
	/// SIZE_MAX when a preflight bound would be exceeded.
	size_t packVertices(const CalypsoGeoscapeColoredLineViewport& viewport)
	{
		if (_commands.size() > COLORED_LINE_COMMAND_CAPACITY)
			return static_cast<size_t>(-1);
		if (_vertices.capacity() < _commands.size() * 2u)
			return static_cast<size_t>(-1);
		_vertices.resize(_commands.size() * 2u);
		for (size_t i = 0; i < _commands.size(); ++i)
		{
			const CalypsoGeoscapeColoredLineCommand& c = _commands[i];
			CalypsoGeoscapeColoredLineVertex& a = _vertices[i * 2u];
			CalypsoGeoscapeColoredLineVertex& b = _vertices[i * 2u + 1u];
			a.x = projectX(c.x1, viewport);
			a.y = projectY(c.y1, viewport);
			b.x = projectX(c.x2, viewport);
			b.y = projectY(c.y2, viewport);
			a.r = c.r; a.g = c.g; a.b = c.b; a.a = c.a;
			b.r = c.r; b.g = c.g; b.b = c.b; b.a = c.a;
		}
		return _vertices.size();
	}

	const CalypsoGeoscapeColoredLineVertex* packedVertices() const
	{
		return _vertices.empty() ? nullptr : &_vertices[0];
	}

	size_t packedVertexBytes() const
	{
		return _vertices.size() * sizeof(CalypsoGeoscapeColoredLineVertex);
	}

private:
	static float projectX(double logicalX, const CalypsoGeoscapeColoredLineViewport& v)
	{
		const double mapped = (v.rectX + logicalX * v.scaleX) / v.displayWidth;
		return static_cast<float>(2.0 * mapped - 1.0);
	}
	static float projectY(double logicalY, const CalypsoGeoscapeColoredLineViewport& v)
	{
		const double mapped = (v.rectY + logicalY * v.scaleY) / v.displayHeight;
		return static_cast<float>(-(2.0 * mapped - 1.0));
	}

	std::vector<CalypsoGeoscapeColoredLineCommand> _commands;
	std::vector<CalypsoGeoscapeColoredLineVertex> _vertices;
};

/// FNV-1a accumulator over exact bit patterns. Floating-point fields hash
/// their IEEE-754 representation, never locale strings or rounded values.
class CalypsoGeoscapeColoredLineSignature
{
public:
	CalypsoGeoscapeColoredLineSignature() : _state(14695981039346656037ull) {}

	void mixBytes(const void* data, size_t length)
	{
		const unsigned char* p = static_cast<const unsigned char*>(data);
		for (size_t i = 0; i < length; ++i)
		{
			_state ^= static_cast<std::uint64_t>(p[i]);
			_state *= 1099511628211ull;
		}
	}

	void mixUint64(std::uint64_t value) { mixBytes(&value, sizeof(value)); }
	void mixInt64(std::int64_t value) { mixBytes(&value, sizeof(value)); }
	void mixDouble(double value)
	{
		std::uint64_t bits = 0;
		std::memcpy(&bits, &value, sizeof(bits));
		mixUint64(bits);
	}
	void mixBool(bool value)
	{
		const std::uint8_t byte = value ? 1u : 0u;
		mixBytes(&byte, sizeof(byte));
	}

	std::uint64_t value() const { return _state; }

private:
	std::uint64_t _state;
};

/// Complete radar/flight snapshot key: every fixed presentation input plus
/// the dynamic campaign signature. The complete key is the correctness
/// backstop; a missing field must show up as stale geometry in tests.
struct CalypsoGeoscapeColoredLineSnapshotKey
{
	std::uint64_t viewportGeneration;
	std::int32_t rectX;
	std::int32_t rectY;
	std::int32_t rectW;
	std::int32_t rectH;
	std::int32_t displayWidth;
	std::int32_t displayHeight;
	double sdlScaleX;
	double sdlScaleY;
	double centreLongitude;
	double centreLatitude;
	double zoomLevel;
	double globeRadius;
	double textureZoom;
	bool hoverEnabled;
	double hoverLongitude;
	double hoverLatitude;
	bool craftRangeEnabled;
	double craftLongitude;
	double craftLatitude;
	double craftRange;
	bool optionRadarLines;
	bool optionFlightPaths;
	bool optionAllRadarsOnBaseBuild;
	std::uint64_t paletteGeneration;
	std::int64_t enemyRadarMode;
	bool debugMode;
	std::uint64_t dynamicSignature;

	bool operator==(const CalypsoGeoscapeColoredLineSnapshotKey& rhs) const
	{
		return std::memcmp(this, &rhs, sizeof(*this)) == 0;
	}
	bool operator!=(const CalypsoGeoscapeColoredLineSnapshotKey& rhs) const
	{
		return !(*this == rhs);
	}
};

/// Generation-separated cache verdicts for one prepared frame.
enum ColoredLinePrepareResult
{
	COLORED_LINE_CACHE_HIT,   ///< key unchanged: reuse commands and VBO
	COLORED_LINE_REBUILT,     ///< key changed: rebuild within capacity
};

/// Content/upload generation bookkeeping shared by the engine owner and
/// the generation contract tests.
class CalypsoGeoscapeColoredLineCacheState
{
public:
	ColoredLinePrepareResult prepare(const CalypsoGeoscapeColoredLineSnapshotKey& key)
	{
		++fingerprintChecks;
		if (_hasCommitted && key == _committedKey)
		{
			++cacheHits;
			return COLORED_LINE_CACHE_HIT;
		}
		_committedKey = key;
		_hasCommitted = true;
		++rebuilds;
		++_contentGeneration;
		return COLORED_LINE_REBUILT;
	}

	/// Called after the interleaved buffer has been uploaded this frame.
	void markUploaded()
	{
		_uploadedGeneration = _contentGeneration;
		_uploadedContextEpoch = _contextEpoch;
	}

	/// Context restoration invalidates raw GL handles and upload validity
	/// only; the immutable CPU snapshot and content generation survive.
	void notifyContextReset() { ++_contextEpoch; }

	std::uint64_t contentGenerationValue() const { return _contentGeneration; }
	bool uploadCurrent() const
	{
		return _uploadedGeneration == _contentGeneration
			&& _uploadedContextEpoch == _contextEpoch
			&& _contentGeneration != 0u;
	}

	// Default-off instrumentation counters (plain integer reads/writes).
	std::uint64_t fingerprintChecks = 0u;
	std::uint64_t cacheHits = 0u;
	std::uint64_t rebuilds = 0u;

private:
	CalypsoGeoscapeColoredLineSnapshotKey _committedKey = CalypsoGeoscapeColoredLineSnapshotKey();
	bool _hasCommitted = false;
	std::uint64_t _contentGeneration = 0u;
	std::uint64_t _uploadedGeneration = 0u;
	std::uint64_t _contextEpoch = 1u;
	std::uint64_t _uploadedContextEpoch = 1u;
};

/// One New Base hover circle emission: centre plus exact range.
struct CalypsoGeoscapeCenterRange
{
	double latitude;
	double longitude;
	double range;
};

static bool calypsoBitsEqual(double a, double b)
{
	std::uint64_t ba = 0, bb = 0;
	std::memcpy(&ba, &a, sizeof(ba));
	std::memcpy(&bb, &b, sizeof(bb));
	return ba == bb;
}

/// In-place first-occurrence filter for New Base hover ranges: drops
/// non-positive ranges, keeps the first occurrence of each bit-exact range,
/// preserves source order, allocates nothing. Returns the new count.
inline size_t calypsoCanonicalizeHoverRanges(double* ranges, size_t count)
{
	size_t out = 0u;
	for (size_t i = 0u; i < count; ++i)
	{
		if (!(ranges[i] > 0.0))
			continue;
		bool seen = false;
		for (size_t j = 0u; j < out; ++j)
		{
			if (calypsoBitsEqual(ranges[j], ranges[i]))
			{
				seen = true;
				break;
			}
		}
		if (!seen)
			ranges[out++] = ranges[i];
	}
	return out;
}

/// Optional cross-centre collapse of bit-exact identical (centre, range)
/// pairs. The engine calls this only after the focused pixel-equivalence
/// proof of the effective opaque colour; distinct centres/ranges survive.
inline size_t calypsoCanonicalizeCenterRanges(CalypsoGeoscapeCenterRange* items,
	size_t count)
{
	size_t out = 0u;
	for (size_t i = 0u; i < count; ++i)
	{
		bool seen = false;
		for (size_t j = 0u; j < out; ++j)
		{
			if (calypsoBitsEqual(items[j].latitude, items[i].latitude)
				&& calypsoBitsEqual(items[j].longitude, items[i].longitude)
				&& calypsoBitsEqual(items[j].range, items[i].range))
			{
				seen = true;
				break;
			}
		}
		if (!seen)
			items[out++] = items[i];
	}
	return out;
}

} // namespace Calypso
} // namespace OpenXcom