#pragma once

#include "tex.h"

namespace tex
{
	class Node;
	template <class T>
	class BasicNode
	{
		using Owner = std::unique_ptr<T>;
		T* _parent = nullptr;
		Owner _next;
		T* _prev = nullptr;
		Owner _first;
		T* _last = nullptr;
	public:
		constexpr BasicNode() = default;
		BasicNode(BasicNode&& b) :
			_parent(b._parent),
			_next(std::move(b._next)), _prev(b._prev),
			_first(std::move(b._first)), _last(b._last)
		{
			for (auto&& e : *this)
				e._parent = static_cast<T*>(this);
		}

		BasicNode& operator=(BasicNode&& b)
		{
			_parent = b._parent;
			_next = std::move(b._next);
			_prev = b._prev;
			_first = std::move(b._first);
			_last = b._last;

			for (auto&& e : *this)
				e._parent = static_cast<T*>(this);

			return *this;
		}

		T* next() const { return _next.get(); }
		T* prev() const { return _prev; }

		Owner detatch();

		void append(Owner child);

		void insertBefore(Owner sibling)
		{
			sibling->_parent = _parent;
			sibling->_prev = _prev;
			Owner& new_owner = _prev ? _prev->_next : _parent->_first;
			sibling->_next = std::move(new_owner);
			_prev = (new_owner = std::move(sibling)).get();
		}

		template <class... Args>
		T& emplace_back(Args&&... args)
		{
			append(std::make_unique<T>(std::forward<Args>(args)...));
			return back();
		}

		T* parent() const { return _parent; }
		bool empty() const { return !_first; }

		T& front() { return *_first; }
		T& back() { return *_last; }
		const T& front() const { return *_first; }
		const T& back() const { return *_last; }

		template <class ValueType>
		class Iterator
		{
			friend class BasicNode<T>;
			ValueType* _it;
			Iterator(ValueType* it) : _it(it) { }
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = void;
			using value_type = ValueType;
			using reference = ValueType & ;
			using pointer = ValueType * ;

			Iterator& operator++() { _it = _it->_next.get(); return *this; }

			reference operator*() const { return *_it; }
			pointer operator->() const { return _it; }

			bool operator==(const Iterator& other) const { return _it == other._it; }
			bool operator!=(const Iterator& other) const { return _it != other._it; }
		};

		using iterator = Iterator<T>;
		using const_iterator = Iterator<const T>;

		iterator begin() { return { _first.get() }; }
		const_iterator begin() const { return { _first.get() }; }
		constexpr       iterator end() { return { nullptr }; }
		constexpr const_iterator end() const { return { nullptr }; }
	};
	template<> std::unique_ptr<Node> BasicNode<Node>::detatch();
	template<> void BasicNode<Node>::append(std::unique_ptr<Node> child);

	class Node : public BasicNode<Node>
	{
		template <class IT>
		float _align_line(const float line_top, const IT first, const IT last);
		bool _collect_line(Context& con, std::vector<Node*>& out);
		void _layout_line(std::vector<tex::Node *> &line, oui::Point &pen, tex::Context & con, float width);
	public:
		std::string data;
		oui::Rectangle box;
		FontType font = FontType::mono;

		Node(std::string data) : data(std::move(data)) { }
		Node(const char* str, size_t len) : data(str, len) { }

		Type type() const
		{
			if (!empty())
				return Type::group;
			switch (data.front())
			{
			case '\\': return Type::command;
			case '%': return Type::comment;
			default:
				return data.front() < 0 || data.front() > ' ' ?
					Type::text : Type::space;
			}
		}

		bool isText() const { return type() == Type::text; }
		bool isSpace() const { return type() == Type::space; }

		int length() const { return isText() ? narrow<int>(data.size()) : 1; }

		struct Layout
		{
			oui::Vector size;
			oui::Align align = oui::topLeft;
			Flow flow = Flow::line;
		};

		Layout updateLayout(Context& con, FontType fonttype, float width);
	private:
	};

	Node tokenize(const char*& in, const char* const end, std::string data, Mode mode);
	inline Node tokenize(std::string_view in)
	{
		auto first = in.data();
		return tokenize(first, first + in.size(), "root", Mode::text);
	}
	std::string readCurly(const char*& in, const char* const end);

}
