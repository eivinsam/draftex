#pragma once

#include "tex.h"

namespace tex
{
	template <class T>
	using Owner = std::unique_ptr<T>;

	template <class To, class From>
	inline auto as(From* from) noexcept
	{
		return dynamic_cast<std::conditional_t<std::is_const_v<From>, const To*, To*>>(from); 
	}

	class Group;
	class Command;
	class Comment;
	class Space;
	class Text;


	namespace details
	{
		template <class V>
		struct ResultType
		{
			template <class N>
			using result_with = decltype(std::declval<V>()(std::declval<N&>()));
			using type = result_with<Group>;
			static_assert(
				std::is_same_v<type, result_with<Command>> &&
				std::is_same_v<type, result_with<Comment>> &&
				std::is_same_v<type, result_with<Space>> &&
				std::is_same_v<type, result_with<Text>>,
				"result must be the same for all node types");
		};

		struct Visitor
		{
			virtual ~Visitor() = default;
			virtual void operator()(Group& group) = 0;
			virtual void operator()(Command& cmd) = 0;
			virtual void operator()(Comment& cmt) = 0;
			virtual void operator()(Space& space) = 0;
			virtual void operator()(Text& text) = 0;
		};

		template <class V, class Result = typename ResultType<V>::type>
		struct GenericVisitor : Visitor
		{
			V& v;

			Result result;

			constexpr GenericVisitor(V& v) : v(v) { }

			void operator()(Group&   n) final { result = v(n); }
			void operator()(Command& n) final { result = v(n); }
			void operator()(Comment& n) final { result = v(n); }
			void operator()(Space&   n) final { result = v(n); }
			void operator()(Text&    n) final { result = v(n); }
		};
		template <class V>
		struct GenericVisitor<V, void> : Visitor
		{
			V& v;

			constexpr GenericVisitor(V& v) : v(v) { }

			void operator()(Group&   n) final { v(n); }
			void operator()(Command& n) final { v(n); }
			void operator()(Comment& n) final { v(n); }
			void operator()(Space&   n) final { v(n); }
			void operator()(Text&    n) final { v(n); }
		};
	}

	enum class Offset : int {};

	class Node
	{
		friend class Group;

		Group* _parent = nullptr;
		Owner<Node> _next;
		Node* _prev = nullptr;

		bool _changed = true;
	protected:
		virtual bool _collect_line(Context& con, std::vector<Node*>& out);

	public:
		using Visitor = details::Visitor;

		enum class Type : char { space, text, group, command, comment };

		std::string data;
		oui::Rectangle box;

		Node() = default;
		virtual ~Node() = default;
		Node(Node&&) = delete;
		Node(const Node&) = delete;

		Node& operator=(Node&&) = delete;
		Node& operator=(const Node&) = delete;

		constexpr bool changed() const { return _changed; }
		void change() noexcept;
		virtual void commit() noexcept { _changed = false; }

		Node* next() const noexcept { return _next.get(); }
		Node* prev() const noexcept { return _prev; }

		Group* parent() const noexcept { return _parent; }
		Node* insertBefore(Owner<Node> sibling) noexcept;
		Node* insertAfter (Owner<Node> sibling) noexcept;
		Owner<Node> detach();
		void remove()
		{
			auto forget = detach();
		}
		Owner<Node> replace(Owner<Node> replacement)
		{
			insertBefore(std::move(replacement));
			return detach();
		}

		virtual Node* expand() { return this; }
		virtual void popArgument(Group& dst);
		virtual Node* getArgument() { return _next->getArgument(); }


		void visit(Visitor&& v) { visit(v); }
		virtual void visit(Visitor&) = 0;

		template <class V, class Result = 
			std::enable_if_t<!std::is_base_of_v<Visitor, V>, typename details::ResultType<V>::type>>
		Result visit(V&& v)
		{
			details::GenericVisitor<V> vrt{ v };
			visit(static_cast<Visitor&>(vrt));
			if constexpr (!std::is_same_v<void, Result>)
				return vrt.result;
		}


		virtual Type type() const = 0;
		bool isSpace() const { return type()==Type::space; }
		bool isText()  const { return type()==Type::text; }

		virtual Node* insertSpace(int offset);
		virtual void insert(int offset, std::string_view text);

		struct Layout
		{
			oui::Vector size;
			oui::Align align = oui::align::min;
			Flow flow = Flow::line;

			constexpr oui::Align2 boxAlign() const
			{
				return flow == Flow::vertical ?
					oui::Align2{ align.c, 0 } :
					oui::Align2{ 0, align.c };
			}

			constexpr auto place(oui::Point    point) const { return boxAlign()(point).size(size); }
			constexpr auto place(oui::Rectangle area) const { return boxAlign()(area) .size(size); }
		};

		virtual Layout updateLayout(Context& con, FontType fonttype, float width);
	};

	inline void tryPopArgument(Node* next, Group& dst)
	{
		if (next == nullptr)
			throw IllFormed("end of group reached while looking for command argument");
		next->popArgument(dst);
	}

	class Group : public Node
	{
		friend class Node;
		Owner<Node> _first;
		Node* _last = nullptr;

		template <class IT>
		float _align_line(const float line_top, const IT first, const IT last);
		void _layout_line(std::vector<tex::Node*> &line, oui::Point &pen, FontType, tex::Context & con, float width);
		bool _collect_line(Context& con, std::vector<Node*>& out) override;
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		Group() = default;

		void commit() noexcept final;

		Type type() const noexcept final { return Type::group; }

		static Owner<Group> make(std::string name) 
		{
			auto result = std::make_unique<Group>();
			result->data = name;
			return result;
		}

		Node* expand() final;
		void popArgument(Group& dst) final { dst.append(detach()); }
		Node* getArgument() noexcept final { return this; }

		Node& append(Owner<Node> child) noexcept;

		bool empty() const noexcept { return !_first; }

		      Node& front()       { return *_first; }
		const Node& front() const { return *_first; }
		      Node& back()        noexcept { return *_last; }
		const Node& back()  const noexcept { return *_last; }

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

			constexpr Iterator& operator++() { _it = _it->_next.get(); return *this; }

			constexpr reference operator*() const { return *_it; }
			constexpr pointer  operator->() const { return _it; }

			constexpr bool operator==(const Iterator& other) const { return _it == other._it; }
			constexpr bool operator!=(const Iterator& other) const { return _it != other._it; }
		};

		using iterator = Iterator<Node>;
		using const_iterator = Iterator<const Node>;

		      iterator begin()       noexcept { return { _first.get() }; }
		const_iterator begin() const noexcept { return { _first.get() }; }
		constexpr       iterator end()       { return { nullptr }; }
		constexpr const_iterator end() const { return { nullptr }; }

		Layout updateLayout(Context& con, FontType fonttype, float width) override;
	};
	inline Node* Node::insertAfter(Owner<Node> sibling) noexcept
	{
		return _next ? 
			_next->insertBefore(std::move(sibling)) : 
			&_parent->append(std::move(sibling)); 
	}

	class Command : public Node
	{
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		Type type() const noexcept final { return Type::command; }

		static auto make() { return std::make_unique<Command>(); }

		Node* expand() final;
		void popArgument(Group& dst) final { dst.append(detach()); }
		Node* getArgument() noexcept final { return this; }
	};

	class Comment : public Node
	{
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		Type type() const noexcept final { return Type::comment; }

		static auto make() { return std::make_unique<Comment>(); }
	};

	class Space : public Node
	{
		bool _collect_line(Context& con, std::vector<Node*>& out) override;
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		Type type() const noexcept final { return Type::space; }

		static auto make() { return std::make_unique<Space>(); }

		Node* insertSpace(int offset) final 
		{ 
			return (offset == 0 && !(prev() && prev()->isSpace())) ? 
				insertBefore(Space::make()) : nullptr;
		}

		void insert(int offset, std::string_view text) final;

		Layout updateLayout(Context& con, FontType fonttype, float width) final;
	};

	class Text : public Node
	{
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		FontType fonttype = FontType::mono;

		Type type() const noexcept final { return Type::text; }

		static auto make() { return std::make_unique<Text>(); }
		static auto make(std::string text)
		{
			auto result = make();
			result->data = std::move(text);
			return result;
		}
		static auto make(std::string_view text) { return make(std::string(text)); }

		void popArgument(Group& dst) final;
		Node* getArgument() noexcept final { return this; }

		Node* insertSpace(int offset) final;
		void insert(int offset, std::string_view text) final
		{
			data.insert(narrow<size_t>(offset), text.data(), text.size());
		}

		Layout updateLayout(Context& con, FontType fonttype, float width) final;
	};


	Owner<Group> tokenize(std::string_view& in, std::string data, Mode mode);
	inline Owner<Group> tokenize(std::string_view in)
	{
		return tokenize(in, "root", Mode::text);
	}
	std::string readCurly(std::string_view& in);

}
