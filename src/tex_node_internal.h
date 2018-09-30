#pragma once

#include <tex_node.h>
#include <find.h>
#include "frozen_with_string_view.h"

namespace tex
{
	using std::move;
	template <class A>
	inline string_view view(const A& s) noexcept { return string_view{ s }; }

	enum class OnEnd : char { pass, match, fail };

	class InputReader
	{
		string_view in;

		Owner<Node> _tokenize_single(const Group& parent, Mode mode, OnEnd on_end);
	public:
		constexpr InputReader(string_view in) noexcept : in(in) { }

		constexpr explicit operator bool() const { return !in.empty(); }
		constexpr char operator*() const { return in.front(); }

		constexpr char pop()
		{
			const char result = in.front();
			in.remove_prefix(1);
			return result;
		}
		constexpr void skip()
		{
			in.remove_prefix(1);
		}

		string readCurly();

		Owner<Group> tokenize_group(string_view type, Mode mode)
		{
			auto result = Group::make(Word(type));
			result->tokenize(*this, mode);
			return result;
		}

		Owner<Node> tokenize_single(const Group& parent, Mode mode, OnEnd on_end)
		{
			return _tokenize_single(parent, mode, on_end);
		}
	};
}
