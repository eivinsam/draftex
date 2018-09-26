#pragma once

#include <vector>
#include <oui.h>

namespace tex
{
	class Node;
	class Context;

	class Paragraph
	{
		std::vector<const Node*> _nodes;
	public:
		void clear() noexcept { _nodes.clear(); }
		void push_back(const Node* node) { _nodes.push_back(node); }

		// returns the resulting height
		float updateLayout(Context& con, Vector pen, float indent, float width);


		class iterator
		{
			friend class Paragraph;
			decltype(_nodes)::iterator _it;

			iterator(decltype(_it) it) noexcept : _it(move(it)) { }
		public:
			iterator() = default;
			iterator & operator++() { ++_it; return *this; }
			iterator & operator--() { --_it; return *this; }

			const Node* operator->() { return *_it; }
			const Node& operator*() { return **_it; }

			bool operator==(const iterator& other) const { return _it == other._it; }
			bool operator!=(const iterator& other) const { return _it != other._it; }
		};
		iterator begin() noexcept { return { _nodes.begin() }; }
		iterator end()   noexcept { return { _nodes.end() }; }
	};

	float layoutParagraph(Context& con, nonull<const Group*> p, float indent, float width);
}
