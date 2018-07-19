#pragma once

#include "tex.h"
#include "express.h"

namespace tex
{
	template <class T>
	using Owner = std::unique_ptr<T>;

	template <class To, class From>
	inline auto as(From* from) noexcept
	{
		return dynamic_cast<std::conditional_t<std::is_const_v<From>, const To*, To*>>(from); 
	}
	template <class To, class From>
	inline auto& as(From& from) noexcept
	{
		return dynamic_cast<std::conditional_t<std::is_const_v<From>, const To&, To&>>(from);
	}


	class Group;
	class Command;
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
				std::is_same_v<type, result_with<Space>> &&
				std::is_same_v<type, result_with<Text>>,
				"result must be the same for all node types");
		};

		struct Visitor
		{
			virtual ~Visitor() = default;
			virtual void operator()(Group& group) = 0;
			virtual void operator()(Command& cmd) = 0;
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
			void operator()(Space&   n) final { v(n); }
			void operator()(Text&    n) final { v(n); }
		};

		template <class T, class Friend>
		class Property
		{
			friend Friend;
			T value;

		public:
			constexpr operator T() const { return value; }
		};
		template <class T, class Friend>
		class Property<T*, Friend>
		{
			friend Friend;
			T* value = nullptr;

			constexpr Property() = default;
			constexpr Property(Property&&) = default;
			constexpr Property(const Property&) = default;
			constexpr Property& operator=(Property&&) = default;
			constexpr Property& operator=(const Property&) = default;

			constexpr Property(T* value) : value(value) { }
			constexpr Property& operator=(T* new_value) { value = new_value; return *this; }
		public:
			constexpr operator T*() const { return value; }
			constexpr operator const T*() const { return value; }
			explicit constexpr operator bool() const { return static_cast<bool>(value); }

			constexpr T* operator->() const { return value; }
			constexpr T& operator*() const { return *value; }

			template <class S> constexpr bool operator==(S&& other) const { return value == other; }
			template <class S> constexpr bool operator!=(S&& other) const { return value != other; }
		};
		template <class T, class Friend>
		class Property<Owner<T>, Friend>
		{
			friend Friend;
			Owner<T> value;

			constexpr Property() = default;
			constexpr Property(Property&&) = default;
			constexpr Property(const Property&) = default;
			constexpr Property& operator=(Property&&) = default;
			constexpr Property& operator=(const Property&) = default;

			constexpr Property(Owner<T> value) : value(std::move(value)) { }
			constexpr Property& operator=(Owner<T> new_value) { value = std::move(new_value); return *this; }

			constexpr Owner<T>& owning() { return value; }
		public:
			constexpr operator T*() const { return value.get(); }
			constexpr operator const T*() const { return value.get(); }
			explicit constexpr operator bool() const { return static_cast<bool>(value); }

			constexpr T* operator->() const { return value.get(); }
			constexpr T& operator*() const { return *value; }

			template <class S> constexpr bool operator==(S&& other) const { return value == other; }
			template <class S> constexpr bool operator!=(S&& other) const { return value != other; }
		};
	}

	enum class Offset : int {};

	class Paragraph;
	class Node
	{
		friend class Group;

		bool _changed = true;
	protected:
		Owner<Node>& _owning_next() { return next.owning(); }
		void _set_parent(Group* p) { parent = p; }
		void _set_prev(Node* p) { prev = p; }


		virtual Text* _this_or_prev_text() noexcept { return prevText(); };
		virtual Text* _this_or_next_text() noexcept { return nextText(); };

		void _insert_before(Owner<Node> sibling);
		void _insert_after(Owner<Node> sibling);
	public:
		details::Property<Group*, Node> parent;
		details::Property<Owner<Node>, Node> next;
		details::Property<Node*, Node> prev;

		using Visitor = details::Visitor;

		enum class Type : char { space, text, group, command, comment };

		std::string data;

		class Shape
		{
		public:
			oui::Vector offset;
			float before;
			float above;
			float after;
			float below;

			constexpr float left() const { return offset.x - before; }
			constexpr float top() const { return offset.y - above; }
			constexpr float right() const { return offset.x + after; }
			constexpr float bottom() const { return offset.y + below; }

			constexpr oui::Point min() const { return { left(), top() }; }
			constexpr oui::Point max() const { return { right(),  bottom() }; }

			constexpr float width() const { return before + after; }
			constexpr float height() const { return above + below; }

			constexpr void width(float w, oui::Align a) { before = w * a.c; after = w - before; }
			constexpr void height(float h, oui::Align a) { above = h * a.c; below = h - above; }
		};
		oui::Vector boxSize;
		Shape box;

		float absLeft() const;
		float absRight() const { return absLeft() + box.width(); }
		float absTop() const;
		float absBottom() const { return absTop() + box.height(); }
		oui::Rectangle absBox() const;

		Node() = default;
		virtual ~Node() = default;
		Node(Node&&) = delete;
		Node(const Node&) = delete;

		Node& operator=(Node&&) = delete;
		Node& operator=(const Node&) = delete;

		constexpr bool changed() const { return _changed; }
		void change() noexcept;
		virtual void commit() noexcept { _changed = false; }

		template <class T> T* insertBefore(Owner<T> sibling) noexcept
		{
			auto raw = sibling.get();
			_insert_before(std::move(sibling));
			return raw;
		}
		template <class T> T* insertAfter(Owner<T> sibling) noexcept
		{
			auto raw = sibling.get();
			_insert_after(std::move(sibling));
			return raw;
		}

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
		virtual Node* getArgument() { return next->getArgument(); }


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

		virtual std::optional<std::string> asEnd() const { return {}; }

		Text* prevText() noexcept;
		Text* nextText() noexcept;

		auto allTextBefore() { return xpr::generator(&Node::prevText, this); }
		auto allTextAfter()  { return xpr::generator(&Node::nextText, this); }

		virtual bool collect(Paragraph& out);
		virtual void updateSize(Context& con, Mode mode, Font font, float width);
		virtual void updateLayout(oui::Vector offset);
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


		Text* _this_or_prev_text() noexcept final { return _last  ? _last ->_this_or_prev_text() : prevText(); }
		Text* _this_or_next_text() noexcept final { return _first ? _first->_this_or_next_text() : nextText(); }

		void _append(Owner<Node> child);
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		Group() = default;

		void commit() noexcept final;

		Type type() const noexcept final { return Type::group; }

		static Owner<Group> make(std::string name);

		Node* expand() final;
		void popArgument(Group& dst) final { dst.append(detach()); }
		Node* getArgument() noexcept final { return this; }

		template <class T>
		T* append(Owner<T> child) noexcept
		{
			auto raw = child.get();
			_append(std::move(child));
			return raw;
		}

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

			constexpr Iterator& operator++() { _it = _it->next; return *this; }

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

		bool collect(Paragraph& out) override;
		void updateSize(Context& con, Mode mode, Font font, float width) override;
		void updateLayout(oui::Vector offset) override;
	};
	inline Text* Node::prevText() noexcept
	{
		return
			prev ? prev->_this_or_prev_text() :
			parent ? parent->prevText() :
			nullptr;
	}
	inline Text* Node::nextText() noexcept
	{
		return
			next ? next->_this_or_next_text() :
			parent ? parent->nextText() :
			nullptr;
	}


	class Command : public Node
	{
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		Type type() const noexcept final { return Type::command; }

		static auto make() { return std::make_unique<Command>(); }
		static auto make(std::string data)
		{
			auto result = make();
			result->data = std::move(data);
			return result;
		}

		Node* expand() final;
		void popArgument(Group& dst) final { dst.append(detach()); }
		Node* getArgument() noexcept final { return this; }

		std::optional<std::string> asEnd() const final
		{
			if (data.size() > 4 && std::string_view(data).substr(0,4) == "end ")
				return data.substr(4);
			else
				return {};
		}
	};

	class Space : public Node
	{
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		Type type() const noexcept final { return Type::space; }

		static auto make() { return std::make_unique<Space>(); }

		bool collect(Paragraph& out) override;
		void updateSize(Context& con, Mode mode, Font font, float width) final;
	};

	class Text : public Node
	{
		Text* _this_or_prev_text() noexcept final { return this; };
		Text* _this_or_next_text() noexcept final { return this; };
	public:
		using Node::visit;
		void visit(Visitor& v) override { v(*this); }

		Font font = { FontType::mono, FontSize::normalsize };
		Mode mode = Mode::text;

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

		Space* insertSpace(int offset);
		int insert(int offset, std::string_view text);

		void updateSize(Context& con, Mode mode, Font font, float width) final;
	};


	Owner<Group> tokenize(std::string_view& in, std::string data, Mode mode);
	inline Owner<Group> tokenize(std::string_view in)
	{
		return tokenize(in, "root", Mode::text);
	}
	std::string readCurly(std::string_view& in);

}
