#pragma once

#include "tex.h"

namespace tex
{
	template <class T>
	using Owner = std::unique_ptr<T>;

	template <class To, class From>
	inline auto as(From* from)
	{
		return dynamic_cast<std::conditional_t<std::is_const_v<From>, const To*, To*>>(from); 
	}

	class Group;
	class Command;
	class Comment;
	class Space;
	class Text;
	class Node
	{
		friend class Group;

		Group* _parent = nullptr;
		Owner<Node> _next;
		Node* _prev = nullptr;
	protected:
		virtual bool _collect_line(Context& con, std::vector<Node*>& out);
	public:
		std::string data;
		oui::Rectangle box;

		Node() = default;

		Node* next() const { return _next.get(); }
		Node* prev() const { return _prev; }

		Group* parent() const { return _parent; }
		void insertBefore(Owner<Node> sibling);
		Owner<Node> detach();
		Owner<Node> replace(Owner<Node> replacement)
		{
			insertBefore(std::move(replacement));
			return detach();
		}

		struct Visitor
		{
			virtual void operator()(Group& group) = 0;
			virtual void operator()(Command& cmd) = 0;
			virtual void operator()(Comment& cmt) = 0;
			virtual void operator()(Space& space) = 0;
			virtual void operator()(Text& text) = 0;
		};

		void visit(Visitor&& v) { visit(v); }
		virtual void visit(Visitor&) = 0;

		virtual bool isSpace() const { return false; }
		virtual bool isText() const { return false; }

		struct Layout
		{
			oui::Vector size;
			oui::Align align = oui::topLeft;
			Flow flow = Flow::line;
		};

		virtual Layout updateLayout(Context& con, FontType fonttype, float width);
	};

	class Group : public Node
	{
		friend class Node;
		Owner<Node> _first;
		Node* _last;

		template <class IT>
		float _align_line(const float line_top, const IT first, const IT last);
		void _layout_line(std::vector<tex::Node*> &line, oui::Point &pen, FontType, tex::Context & con, float width);
		bool _collect_line(Context& con, std::vector<Node*>& out) override;
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		Group() = default;

		Group(Group&& b)
			: Node(std::move(b)), _first(std::move(b._first)), _last(b._last)
		{
			for (auto&& n : *this)
				n._parent = this;
		}

		static Owner<Group> create(std::string name) 
		{
			auto result = std::make_unique<Group>();
			result->data = name;
			return result;
		}

		Group& operator=(Group&& b)
		{
			static_cast<Node&>(*this) = std::move(b);
			_first = std::move(b._first);
			_last = b._last;
			for (auto&& n : *this)
				n._parent = this;
		}


		Node& append(Owner<Node> child);

		bool empty() const { return !_first; }

		Node& front() { return *_first; }
		Node& back() { return *_last; }
		const Node& front() const { return *_first; }
		const Node& back() const { return *_last; }

		template <class ValueType>
		class Iterator
		{
			friend class Group;
			ValueType* _it;
			constexpr Iterator(ValueType* it) : _it(it) { }
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

		using iterator = Iterator<Node>;
		using const_iterator = Iterator<const Node>;

		iterator begin() { return { _first.get() }; }
		const_iterator begin() const { return { _first.get() }; }
		constexpr       iterator end() { return { nullptr }; }
		constexpr const_iterator end() const { return { nullptr }; }

		Layout updateLayout(Context& con, FontType fonttype, float width) override;
	};

	class Command : public Node
	{
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		static auto make() { return std::make_unique<Command>(); }
	};

	class Comment : public Node
	{
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		static auto make() { return std::make_unique<Comment>(); }
	};

	class Space : public Node
	{
		bool _collect_line(Context& con, std::vector<Node*>& out) override;
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		static auto make() { return std::make_unique<Space>(); }

		bool isSpace() const final { return true; }

		Layout updateLayout(Context& con, FontType fonttype, float width) final;
	};

	class Text : public Node
	{
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		FontType font;

		static auto make() { return std::make_unique<Text>(); }
		static auto make(std::string text)
		{
			auto result = make();
			result->data = std::move(text);
			return result;
		}

		bool isText() const final { return true; }

		Layout updateLayout(Context& con, FontType fonttype, float width) final;
	};


	Owner<Group> tokenize(const char*& in, const char* const end, std::string data, Mode mode);
	inline Owner<Group> tokenize(std::string_view in)
	{
		auto first = in.data();
		return tokenize(first, first + in.size(), "root", Mode::text);
	}
	std::string readCurly(const char*& in, const char* const end);

}
