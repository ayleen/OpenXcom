#pragma once
// F01 visual catalog (T14 reader, T17 art): ruleType to HD asset mapping.
// Graphic-only schema (plan section 8.4): keys, paths, UV/size metadata.
// No prices, requirements, coordinates, stats, callbacks, or URLs.
// The JSON subset parser below is strict and dependency-free so native unit
// tests prove every acceptance and rejection; FileMap loading lives in the
// Emscripten-guarded .cpp. Not #ifdef-guarded (pure-helper convention).

#include <cstdint>
#include <map>
#include <string>

#include "CalypsoHdImageModel.h"

namespace OpenXcom
{
namespace Calypso
{

inline constexpr const char *kCalypsoBaseCatalogPath = "Resources/basescape/catalog.json";
inline constexpr const char *kCalypsoBaseCatalogVersion = "hd-basescape-1";
inline constexpr const char *kCalypsoBaseAssetPrefix = "Resources/basescape/";

struct CalypsoBaseVisualCatalog
{
	bool loaded = false;
	std::uint32_t generation = 0;
	std::string background;
	std::string emptyCell;
	std::string construction;
	std::string connectorH;
	std::string connectorV;
	std::string genericFacility;
	std::map<std::string, std::string> facilityReady;
	std::map<std::string, std::string> craftArt;
	std::map<std::string, std::string> cardArt;
};

inline bool calypsoBaseAssetPathValid(const std::string &path)
{
	if (path.empty())
	{
		return false;
	}
	if (path.compare(0, 20, kCalypsoBaseAssetPrefix) != 0)
	{
		return false;
	}
	if (path.find("..") != std::string::npos)
	{
		return false;
	}
	if (path.find("://") != std::string::npos)
	{
		return false;
	}
	return true;
}

namespace BaseCatalogJson
{

struct Value
{
	enum class Type { Null, Str, Int, Obj };
	Type type = Type::Null;
	std::string str;
	long long num = 0;
	std::map<std::string, Value> obj;
};

// Byte literals name ASCII directly (34 quote, 47 slash, 58 colon, 44 comma,
// 45 minus, 46 dot, 92 backslash, 110 n, 116 t, 123 open-brace, 125 close,
// 48-57 digits, 101/69 exponent). Accepted grammar: objects, strings with
// escapes, integers. Arrays, floats, literals, and nesting past 8 rejected.
struct Parser
{
	const char *p;
	const char *end;
	std::string error;

	bool skipSpace()
	{
		while (p != end && (*p == 32 || *p == 9 || *p == 10 || *p == 13))
		{
			++p;
		}
		return p != end;
	}

	bool take(char c)
	{
		if (!skipSpace() || *p != c)
		{
			return false;
		}
		++p;
		return true;
	}

	bool parseString(std::string &out)
	{
		if (!take(34))
		{
			error = "expected string";
			return false;
		}
		out.clear();
		while (p != end && *p != 34)
		{
			if (*p == 92)
			{
				++p;
				if (p == end)
				{
					error = "dangling escape";
					return false;
				}
				switch (*p)
				{
				case 34: out.push_back(34); break;
				case 92: out.push_back(92); break;
				case 47: out.push_back(47); break;
				case 110: out.push_back(10); break;
				case 116: out.push_back(9); break;
				default:
					error = "unsupported escape";
					return false;
				}
				++p;
			}
			else if (*p >= 0 && *p < 32)
			{
				error = "unescaped control character";
				return false;
			}
			else
			{
				out.push_back(*p);
				++p;
			}
		}
		if (p == end)
		{
			error = "unterminated string";
			return false;
		}
		++p;
		return true;
	}

	bool parseValue(Value &out, int depth)
	{
		if (depth > 8)
		{
			error = "nesting too deep";
			return false;
		}
		if (!skipSpace())
		{
			error = "unexpected end";
			return false;
		}
		if (*p == 34)
		{
			out.type = Value::Type::Str;
			return parseString(out.str);
		}
		if ((*p >= 48 && *p <= 57) || *p == 45)
		{
			out.type = Value::Type::Int;
			const char *start = p;
			if (*p == 45)
			{
				++p;
			}
			if (p == end || *p < 48 || *p > 57)
			{
				error = "bad number";
				return false;
			}
			while (p != end && *p >= 48 && *p <= 57)
			{
				++p;
			}
			if (p != end && (*p == 46 || *p == 101 || *p == 69))
			{
				error = "floats are not allowed";
				return false;
			}
			try
			{
				out.num = std::stoll(std::string(start, p));
			}
			catch (...)
			{
				error = "bad number";
				return false;
			}
			return true;
		}
		if (*p == 123)
		{
			out.type = Value::Type::Obj;
			++p;
			if (take(125))
			{
				return true;
			}
			while (true)
			{
				std::string key;
				if (!parseString(key))
				{
					return false;
				}
				if (!take(58))
				{
					error = "expected colon";
				return false;
				}
				if (out.obj.find(key) != out.obj.end())
				{
					error = "duplicate key";
				return false;
				}
				Value child;
			if (!parseValue(child, depth + 1))
				{
					return false;
				}
				out.obj[key] = child;
				if (take(125))
				{
					return true;
				}
				if (!take(44))
				{
					error = "expected comma";
				return false;
			}
			}
		}
		error = "unexpected character";
		return false;
	}
};

} // namespace BaseCatalogJson

inline bool calypsoParseBaseVisualCatalog(const std::string &text,
	CalypsoBaseVisualCatalog &out, std::string &error)
{
	using Json = BaseCatalogJson::Value;
	out = CalypsoBaseVisualCatalog{};
	BaseCatalogJson::Parser parser{text.c_str(), text.c_str() + text.size(), {}};
	Json root;
	if (!parser.parseValue(root, 0) || root.type != Json::Type::Obj)
	{
		error = parser.error.empty() ? "root must be an object" : parser.error;
		return false;
	}
	if (parser.skipSpace())
	{
		error = "trailing content";
		return false;
	}
	static const char *known[] = {"schema", "version", "generation", "background",
		"emptyCell", "construction", "connectorH", "connectorV", "genericFacility",
		"facilities", "crafts", "cards"};
	for (const auto &kv : root.obj)
	{
		bool ok = false;
		for (const char *k : known)
		{
			ok = ok || kv.first == k;
		}
		if (!ok)
		{
			error = "unknown field: " + kv.first;
			return false;
		}
	}
	const auto need = [&](const char *key, Json::Type type) -> const Json * {
		auto it = root.obj.find(key);
		if (it == root.obj.end() || it->second.type != type)
		{
			error = std::string("missing or invalid field: ") + key;
			return nullptr;
		}
		return &it->second;
	};
	const Json *schema = need("schema", Json::Type::Int);
	const Json *version = need("version", Json::Type::Str);
	const Json *generation = need("generation", Json::Type::Int);
	if (schema == nullptr || version == nullptr || generation == nullptr)
	{
		return false;
	}
	if (schema->num != 1)
	{
		error = "schema must be 1";
		return false;
	}
	if (version->str != kCalypsoBaseCatalogVersion)
	{
		error = "unsupported version";
		return false;
	}
	if (generation->num < 0 || generation->num > 1000000)
	{
		error = "bad generation";
		return false;
	}
	const char *paths[] = {"background", "emptyCell", "construction", "connectorH",
		"connectorV", "genericFacility"};
	std::string *slots[] = {&out.background, &out.emptyCell, &out.construction,
		&out.connectorH, &out.connectorV, &out.genericFacility};
	for (int i = 0; i < 6; ++i)
	{
		const Json *v = need(paths[i], Json::Type::Str);
		if (v == nullptr)
		{
			return false;
		}
		if (!calypsoBaseAssetPathValid(v->str))
		{
			error = std::string("bad asset path: ") + paths[i];
			return false;
		}
		*slots[i] = v->str;
	}
	const Json *facilities = need("facilities", Json::Type::Obj);
	if (facilities == nullptr)
	{
		return false;
	}
	for (const auto &kv : facilities->obj)
	{
		if (kv.second.type != Json::Type::Obj || kv.second.obj.size() != 1)
		{
			error = "facility entry must be exactly ready: " + kv.first;
			return false;
		}
		auto it = kv.second.obj.find("ready");
		if (it == kv.second.obj.end() || it->second.type != Json::Type::Str)
		{
			error = "facility entry must be exactly ready: " + kv.first;
			return false;
		}
		if (!calypsoBaseAssetPathValid(it->second.str))
		{
			error = "bad facility path: " + kv.first;
			return false;
		}
		out.facilityReady[kv.first] = it->second.str;
	}
	for (int m = 0; m < 2; ++m)
	{
		const char *mapKey = m == 0 ? "crafts" : "cards";
		auto it = root.obj.find(mapKey);
		if (it == root.obj.end())
		{
			continue;
		}
		if (it->second.type != Json::Type::Obj)
		{
			error = std::string("invalid map: ") + mapKey;
			return false;
		}
		std::map<std::string, std::string> *target = m == 0 ? &out.craftArt : &out.cardArt;
		for (const auto &kv : it->second.obj)
		{
			if (kv.second.type != Json::Type::Str || !calypsoBaseAssetPathValid(kv.second.str))
			{
				error = std::string("bad art path in ") + mapKey + ": " + kv.first;
				return false;
			}
			(*target)[kv.first] = kv.second.str;
		}
	}
	out.generation = static_cast<std::uint32_t>(generation->num);
	out.loaded = true;
	return true;
}

inline std::string calypsoBaseFacilityImage(const CalypsoBaseVisualCatalog &catalog,
	const std::string &ruleType, bool &known)
{
	auto it = catalog.facilityReady.find(ruleType);
	if (it != catalog.facilityReady.end())
	{
		known = true;
		return it->second;
	}
	known = false;
	return catalog.genericFacility;
}

inline std::string calypsoBaseCraftImage(const CalypsoBaseVisualCatalog &catalog,
	const std::string &artKey)
{
	auto it = catalog.craftArt.find(artKey);
	return it != catalog.craftArt.end() ? it->second : std::string();
}

inline std::string calypsoBaseCardImage(const CalypsoBaseVisualCatalog &catalog,
	const std::string &actionId)
{
	auto it = catalog.cardArt.find(actionId);
	return it != catalog.cardArt.end() ? it->second : std::string();
}

#ifdef __EMSCRIPTEN__
bool calypsoLoadBaseVisualCatalog(CalypsoBaseVisualCatalog &out, std::string &error);
#endif

inline CalypsoHdImageDescriptor calypsoBaseCatalogImage(
	const CalypsoBaseVisualCatalog &catalog, const std::string &source)
{
	CalypsoHdImageDescriptor desc;
	desc.source = source;
	desc.generation = catalog.generation;
	return desc;
}

} // namespace Calypso
} // namespace OpenXcom
