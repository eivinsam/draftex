#pragma once

#include <vector>
#include <oui.h>

namespace tex
{
	class Node;
	class Paragraph
	{
		std::vector<Node*> _nodes;

	public:

		void clear() { _nodes.clear(); }
		void push_back(Node* node) { _nodes.push_back(node); }

		// returns the resulting height
		float updateLayout(oui::Vector pen, float indent, float width);


		class iterator
		{
			friend class Paragraph;
			decltype(_nodes)::iterator _it;

			iterator(decltype(_it) it) : _it(move(it)) { }
		public:
			iterator() = default;
			iterator & operator++() { ++_it; return *this; }
			iterator & operator--() { --_it; return *this; }

			Node* operator->() { return *_it; }
			Node& operator*() { return **_it; }

			bool operator==(const iterator& other) const { return _it == other._it; }
			bool operator!=(const iterator& other) const { return _it != other._it; }
		};
		iterator begin() { return { _nodes.begin() }; }
		iterator end() { return { _nodes.end() }; }
	};
}
