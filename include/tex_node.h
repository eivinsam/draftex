#pragma once

#include "tex.h"
#include "express.h"
#include <small_string.h>

namespace tex
{
	class Group;
	class Command;
	class Space;
	class Text;

	class Paragraph;

	class Node
	{
		friend class Group;

		bool _changed = true;
	protected:

		Owner<Node>& _owning_next() { return next.owning(); }
		void _set_parent(Group* p) { parent = p; }
		void _set_prev(Node* p) { prev = p; }

		virtual Text* _this_or_prev_text() noexcept { return prevText(); }
		virtual Text* _this_or_next_text() noexcept { return nextText(); }

		virtual Text* _this_or_next_stop() noexcept { return nextStop(); }
		virtual Text* _this_or_prev_stop() noexcept { return prevStop(); }

		void _insert_before(Owner<Node> sibling);
		void _insert_after(Owner<Node> sibling);

		Box _box;
	public:
		using string = SmallString;

		details::Property<Group*, Node> parent;
		details::Property<Owner<Node>, Node> next;
		details::Property<Node*, Node> prev;

		enum class Type : char { space, text, group, command };


		void layoutOffset(Vector offset) { _box.offset = offset; }
		const Box& layoutBox()  const { return _box; }
		virtual const Box& contentBox() const { return _box; }

		Vector absOffset() const;
		float absLeft()   const { return absOffset().x - contentBox().before; }
		float absRight()  const { return absOffset().x + contentBox().after; }
		float absTop()    const { return absOffset().y - contentBox().above; }
		float absBottom() const { return absOffset().y + contentBox().below; }
		Rectangle absBox() const
		{
			auto& cbox = contentBox();
			const auto off = absOffset();
			return { Point(-cbox.before, -cbox.above) + off, Point(cbox.after, cbox.below) + off };
		}

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

		virtual void enforceRules() { };


		virtual Type type() const noexcept = 0;
		virtual Flow flow() const noexcept = 0;

		virtual std::optional<string> asEnd() const { return {}; }

		Text* prevText() noexcept;
		Text* nextText() noexcept;

		Text* prevStop() noexcept;
		Text* nextStop() noexcept;

		auto allTextBefore() { return xpr::generator(&Node::prevText, this); }
		auto allTextAfter()  { return xpr::generator(&Node::nextText, this); }

		virtual bool collect(Paragraph& out);
		virtual Box& updateSize(Context& con, Mode mode, Font font, float width) = 0;
		virtual void render(Context& con, Vector offset) const = 0;

		virtual void serialize(std::ostream& out) const = 0;
		void serialize(std::ostream&& out) { serialize(out); }
	};
	inline bool space(const Node& n) { return n.type() == Node::Type::space; }
	inline bool text(const Node& n) { return n.type() == Node::Type::text; }
	inline bool space(const Node* n) { return n && space(*n); }
	inline bool text(const Node* n) { return n && text(*n); }
	inline bool nullOrSpace(const Node* n) { return !n || space(*n); }
	inline bool nullOrText(const Node* n) { return !n || text(*n); }
	template <class T>
	inline bool space(const Owner<T>& n) { return n && space(*n); }
	template <class T>
	inline bool text(const Owner<T>& n) { return n && text(*n); }
	template <class T>
	inline bool nullOrSpace(const Owner<T>& n) { return !n || space(*n); }
	template <class T>
	inline bool nullOrText(const Owner<T>& n) { return !n || text(*n); }

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


		Text* _this_or_prev_text() noexcept override { return _last  ? _last ->_this_or_prev_text() : prevText(); }
		Text* _this_or_next_text() noexcept override { return _first ? _first->_this_or_next_text() : nextText(); }
		virtual Text* _exit_this_or_next_text() noexcept { return nextText(); }
		virtual Text* _exit_this_or_prev_text() noexcept { return prevText(); }

		Text* _this_or_prev_stop() noexcept override { return _last  ? _last ->_this_or_prev_stop() : prevStop(); }
		Text* _this_or_next_stop() noexcept override { return _first ? _first->_this_or_next_stop() : nextStop(); }
		virtual Text* _exit_this_or_next_stop() noexcept { return nextStop(); }
		virtual Text* _exit_this_or_prev_stop() noexcept { return prevStop(); }

		void _append(Owner<Node> child);
	protected:
		void _enforce_child_rules() { for (auto&& e : *this) e.enforceRules(); }
	public:
		Group() = default;

		void commit() noexcept final;

		Type type() const noexcept final { return Type::group; }
		Flow flow() const noexcept override { return Flow::line; }


		static Owner<Group> make(string name);

		virtual void tokenize(std::string_view& in, Mode mode);
		virtual bool terminatedBy(std::string_view token) const = 0;

		Node* expand() final;
		void popArgument(Group& dst) final { dst.append(detach()); }
		Node* getArgument() noexcept final { return this; }

		void enforceRules() override;

		template <class T>
		T* append(Owner<T> child) noexcept
		{
			auto raw = child.get();
			_append(std::move(child));
			return raw;
		}

		bool contains(Node* n) const
		{
			for (; n != nullptr; n = n->parent())
				if (n == this)
					return true;
			return false;
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

			constexpr Iterator& operator++() { _it = _it->next(); return *this; }

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
		Box& updateSize(Context& con, Mode mode, Font font, float width) override;
		void render(Context& con, Vector offset) const override;

		void serialize(std::ostream& out) const override;
		void serialize(std::ostream&& out) { serialize(out); }
	};
	inline Text* Node::prevText() noexcept
	{
		return
			prev ? prev->_this_or_prev_text() :
			parent ? parent->_exit_this_or_next_text() :
			nullptr;
	}
	inline Text* Node::nextText() noexcept
	{
		return
			next ? next->_this_or_next_text() :
			parent ? parent->_exit_this_or_next_text() :
			nullptr;
	}
	inline Text* Node::prevStop() noexcept
	{
		return
			prev ? prev->_this_or_prev_stop() :
			parent ? parent->_exit_this_or_prev_stop() :
			nullptr;
	}
	inline Text* Node::nextStop() noexcept
	{
		return
			next ? next->_this_or_next_stop() :
			parent ? parent->_exit_this_or_next_stop() :
			nullptr;
	}
	inline Vector Node::absOffset() const
	{ 
		Vector result = contentBox().offset;
		for (const Group* n = parent(); n; n = n->parent())
			result += n->contentBox().offset;
		return result;
	}

	class Command : public Node
	{
	public:
		string cmd;

		Type type() const noexcept final { return Type::command; }
		Flow flow() const noexcept final { return Flow::line; }

		static auto make() { return std::make_unique<Command>(); }
		static auto make(string data)
		{
			auto result = make();
			result->cmd = std::move(data);
			return result;
		}

		Node* expand() final;
		void popArgument(Group& dst) final { dst.append(detach()); }
		Node* getArgument() noexcept final { return this; }

		std::optional<string> asEnd() const final
		{
			if (cmd.size() > 4 && std::string_view(cmd).substr(0,4) == "end ")
				return cmd.substr(4);
			else
				return {};
		}

		Box& updateSize(Context& con, Mode, Font font, float width) final;

		void render(tex::Context& con, Vector offset) const final;

		void serialize(std::ostream& out) const override { out << '\\' << cmd; }
		void serialize(std::ostream&& out) { serialize(out); }
	};

	class Space : public Node
	{
	public:
		string space;

		Type type() const noexcept final { return Type::space; }
		Flow flow() const noexcept final { return count(space, '\n') < 2 ? Flow::line : Flow::vertical; }

		static auto make() { return std::make_unique<Space>(); }

		bool collect(Paragraph& out) override;
		Box& updateSize(Context& con, Mode mode, Font font, float width) final;
		void render(tex::Context&, Vector) const final { } // does nothing

		void serialize(std::ostream& out) const override { out << space; }
		void serialize(std::ostream&& out) { serialize(out); }
	};

	class Text : public Node
	{
		Text* _this_or_prev_text() noexcept final { return this; };
		Text* _this_or_next_text() noexcept final { return this; };

	public:
		string text;

		Font font = { FontType::mono, FontSize::normalsize };
		Mode mode = Mode::text;

		Type type() const noexcept final { return Type::text; }
		Flow flow() const noexcept final { return Flow::line; }

		static auto make() { return std::make_unique<Text>(); }
		static auto make(string text)
		{
			auto result = make();
			result->text = std::move(text);
			return result;
		}

		void popArgument(Group& dst) final;
		Node* getArgument() noexcept final { return this; }

		Space* insertSpace(int offset);
		int insert(int offset, std::string_view text);

		Box& updateSize(Context& con, Mode mode, Font font, float width) final;
		void render(tex::Context& con, oui::Vector offset) const final;

		void serialize(std::ostream& out) const override { out << text; }
		void serialize(std::ostream&& out) { serialize(out); }
	};

	class Par : public Group
	{
	public:
		enum class Type : unsigned char { simple, title, author, section, subsection };
		static friend std::string_view name(Type t)
		{
			static const char* cmd_name[] =
			{
				"",
				"\\title",
				"\\author",
				"\\section",
				"\\subsection"
			};
			return gsl::at(cmd_name, static_cast<std::underlying_type_t<Par::Type>>(t));
		}
	private:
		static constexpr auto _code(Type t) { return static_cast<unsigned char>(t); }

		Type _type;
		Font _font;
		string _pretitle;
	protected:
		float _parindent = 0;
	public:
		Par(string token);

		void enforceRules() final { _enforce_child_rules(); }

		bool terminatedBy(std::string_view) const final { return false; }

		void partype(Type t);
		Type partype() { return _type; }

		bool collect(Paragraph&) final { return false; }

		Box& updateSize(Context& con, Mode mode, Font font, float width) override;
		void render(Context& con, Vector offset) const final;
		void serialize(std::ostream& out) const final
		{
			out << name(_type);

			Group::serialize(out);
		}
	};


	inline Owner<Group> tokenize(std::string_view in)
	{
		auto result = Group::make("root");
		result->tokenize(in, Mode::text);
		return result;
	}
}
