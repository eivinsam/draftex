#pragma once

#include <string_view>
#include <frozen/unordered_set.h>
#include <frozen/unordered_map.h>

#pragma warning(disable: 4307)
template <>
struct frozen::elsa<std::string_view>
{
	constexpr uint64_t operator()(std::string_view value, uint64_t d) const
	{

		for (char c : value)
			d = (0x01000193 * d) ^ c;
		return d;
	}
};
