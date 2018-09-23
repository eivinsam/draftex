#pragma once

#include <string_view>
#pragma warning(push)
#pragma warning(disable: 26495 26432 26481 26493 26445 26496 26472 26485 26482 26446 26429)
#include <frozen/unordered_set.h>
#include <frozen/unordered_map.h>
#pragma warning(pop)

#pragma warning(disable: 4307)
template <>
struct frozen::elsa<std::string_view>
{
	constexpr uint64_t operator()(std::string_view value, uint64_t d) const
	{

		for (const char c : value)
			d = (0x01000193 * d) ^ c;
		return d;
	}
};
