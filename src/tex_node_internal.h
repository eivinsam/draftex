#pragma once

#include <tex_node.h>
#include <find.h>
#include "frozen_with_string_view.h"

namespace tex
{
	using std::make_unique;

	inline string_view view(const string& s) noexcept { return string_view{ s }; }

	enum class OnEnd : char { pass, match, fail };

	inline Owner<Group> tokenize(string_view& in, string type, Mode mode)
	{
		auto result = Group::make(std::move(type));
		result->tokenize(in, mode);
		return result;
	}

	Owner<Node> tokenize_single(string_view&, Group&, Mode, OnEnd);
}
