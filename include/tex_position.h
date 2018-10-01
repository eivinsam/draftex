#pragma once

#include "tex_node.h"

namespace tex
{
	inline constexpr auto subview(std::string_view text, size_t off, size_t count) { return text.substr(off, count); }

	struct Position
	{
		const Text* node = nullptr;
		int offset = 0;

		constexpr Position() = default;
		constexpr Position(const Text* node, int offset) : node(node), offset(offset) { }

		void recede();
		void advance();

		Position prev() const { auto p = *this; p.recede();  return p; }
		Position next() const { auto p = *this; p.advance(); return p; }

		Position operator+(int) const;

		float xOffset(tex::Context& con) const;

		string_view character() const;
		int characterLength() const;


		int maxOffset() const noexcept { Expects(node != nullptr); return node->text().size(); }

		constexpr bool operator==(const Position& p) const { return node == p.node && offset == p.offset; }
		constexpr bool operator!=(const Position& p) const { return !operator==(p); }

		void foo();

		bool valid() const
		{
			return node != nullptr &&
				offset >= 0 &&
				offset <= node->text().size();
		}

		bool atNodeStart() const { return offset == 0; }
		bool atNodeEnd() const { return offset == node->text().size(); }
	};

	template <class PT>
	struct unwrapper
	{ 
		static auto apply(const PT& p) { return p.get(); }
	};
	template <class T>
	struct unwrapper<T*>
	{
		static constexpr auto apply(T* p) { return p; }
	};
	template <class PT>
	auto unwrap(const PT& p) { return unwrapper<PT>::apply(p); }

	template <class PT> constexpr Position start(const PT& node) { return { unwrap(node), 0 }; }
	template <class PT> constexpr Position   end(const PT& node) { return { unwrap(node), node->text().size() }; }
}
