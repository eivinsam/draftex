#pragma once

#include "tex_context.h"
#include "express.h"

namespace tex
{
	class Group;
	class Command;
	class Text;
	class Bibliography;

	class Paragraph;
	class InputReader;

	using intrusive::nonull;
	
	class Node : public intrusive::refcount
	{
		friend class Group;

		bool _changed = true;
	protected:
		virtual Text* _this_or_next_stop() noexcept { return nextStop(); }
		virtual Text* _this_or_prev_stop() noexcept { return prevStop(); }

		template <class T, class... Args>
		static auto make(Args&&... args) { return intrusive::refcount::make<T>(std::forward<Args>(args)...); }

		Box _box;
	public:
		using string = SmallString;

		intrusive::list_element<Node, Group> group;
		string space_after;

		enum class Type : char { text, group, command };

		nonull<Owner<Node>> replaceWith(nonull<Owner<Node>> p) noexcept;
		nonull<Owner<Node>> detachFromGroup() noexcept;
		void removeFromGroup() noexcept;
		template <class T> nonull<T*> insertBeforeThis(nonull<Owner<T>> p) noexcept;
		template <class T> nonull<T*> insertAfterThis(nonull<Owner<T>> p)  noexcept;
		template <class T> nonull<T*> insertBeforeThis(Owner<T> p) noexcept{ return insertBeforeThis(nonull{ move(p) }); }
		template <class T> nonull<T*> insertAfterThis(Owner<T> p)  noexcept{ return insertAfterThis(nonull{ move(p) }); }

		std::vector<nonull<Node*>> parents();

		constexpr void layoutOffset(Vector offset) noexcept { _box.offset = offset; }
		        const Box& layoutBox()  const noexcept { return _box; }
		virtual const Box& contentBox() const noexcept { return _box; }

		Vector absOffset() const noexcept;
		float absLeft()   const noexcept { return absOffset().x - contentBox().before; }
		float absRight()  const noexcept { return absOffset().x + contentBox().after; }
		float absTop()    const noexcept { return absOffset().y - contentBox().above; }
		float absBottom() const noexcept { return absOffset().y + contentBox().below; }
		Rectangle absBox() const noexcept
		{
			auto& cbox = contentBox();
			const auto off = absOffset();
			return { Point(-cbox.before, -cbox.above) + off, Point(cbox.after, cbox.below) + off };
		}
		constexpr void widen(float w) noexcept { _box.after += w; }

		Node() = default;
		virtual ~Node() = default;
		Node(Node&&) = delete;
		Node(const Node&) = delete;

		Node& operator=(Node&&) = delete;
		Node& operator=(const Node&) = delete;

		constexpr bool changed() const { return _changed; }
		void markChange() noexcept;
		virtual void commit() noexcept { _changed = false; }


		virtual Node* expand() { return this; }
		virtual void popArgument(Group& dst);
		virtual Node* getArgument() { return group.next()->getArgument(); }

		virtual void enforceRules() noexcept { };


		virtual Type type() const noexcept = 0;
		virtual Flow flow() const noexcept = 0;

		virtual std::optional<string> asEnd() const noexcept { return {}; }

		virtual Text* prevTextInclusive() noexcept { return prevText(); }
		virtual Text* nextTextInclusive() noexcept { return nextText(); }
		Text* prevText() noexcept;
		Text* nextText() noexcept;

		Text* prevStop() noexcept;
		Text* nextStop() noexcept;

		auto allTextBefore() { return xpr::generator(&Node::prevText, this); }
		auto allTextAfter()  { return xpr::generator(&Node::nextText, this); }

		virtual Bibliography* bibliography() const noexcept;

		virtual bool collect(Paragraph& out);
		virtual Box& updateLayout(Context& con) = 0;
		virtual void render(Context& con, Vector offset) const = 0;

		virtual void serialize(std::ostream& out) const = 0;
	};
	inline bool text(const Node& n) noexcept { return n.type() == Node::Type::text; }
	inline bool text(const Node* n) noexcept { return n && text(*n); }
	inline bool nullOrText(const Node* n) noexcept { return !n || text(*n); }
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

	class Group : public Node, public intrusive::list<Node, Group, &Node::group>
	{
		friend class Node;

		virtual Text* _exit_this_or_next_text() noexcept { return nextText(); }
		virtual Text* _exit_this_or_prev_text() noexcept { return prevText(); }

		Text* _this_or_prev_stop() noexcept override { return !empty() ? rbegin()->_this_or_prev_stop() : prevStop(); }
		Text* _this_or_next_stop() noexcept override { return !empty() ?  begin()->_this_or_next_stop() : nextStop(); }
		virtual Text* _exit_this_or_next_stop() noexcept { return nextStop(); }
		virtual Text* _exit_this_or_prev_stop() noexcept { return prevStop(); }

	protected:
		void _enforce_child_rules() noexcept { for (auto&& e : *this) e.enforceRules(); }
	public:
		Group() = default;

		void commit() noexcept final;

		Type type() const noexcept final { return Type::group; }
		Flow flow() const noexcept override { return Flow::line; }

		Text* prevTextInclusive() noexcept override { return !empty() ? rbegin()->prevTextInclusive() : prevText(); }
		Text* nextTextInclusive() noexcept override { return !empty() ?  begin()->nextTextInclusive() : nextText(); }

		static Owner<Group> make(string name) noexcept;

		virtual void tokenize(InputReader& in, Mode mode);
		void tokenizeText(string_view);
		virtual bool terminatedBy(std::string_view token) const noexcept = 0;

#pragma warning(push)
#pragma warning(disable: 26440)
		Node* expand() override;
		void popArgument(Group& dst) final { dst.append(group->detach(this)); }
#pragma warning(pop)
		Node* getArgument() noexcept final { return this; }

		void enforceRules() noexcept override;

		constexpr bool contains(Node* n) const
		{
			for (; n != nullptr; n = n->group())
				if (n == this)
					return true;
			return false;
		}

		constexpr Node& front() const { return *begin(); }
		constexpr Node& back()  const { return *rbegin(); }

		bool collect(Paragraph& out) override;
		Box& updateLayout(Context& con) override;
		void render(Context& con, Vector offset) const override;

		void serialize(std::ostream& out) const override;
	};

	inline nonull<Owner<Node>> Node::replaceWith(nonull<Owner<Node>> p) noexcept
	{ 
		insertBeforeThis(move(p));
		return detachFromGroup();
	}

	inline nonull<Owner<Node>> Node::detachFromGroup() noexcept { return group->detach(this); }
	inline void Node::removeFromGroup() noexcept { group->remove(this); }
	template<class T>
	inline nonull<T*> Node::insertBeforeThis(nonull<Owner<T>> p) noexcept { return group->insert_before(this, move(p)); }
	template<class T>
	inline nonull<T*> Node::insertAfterThis(nonull<Owner<T>> p) noexcept { return group->insert_after(this, move(p)); }
	inline Text* Node::prevText() noexcept
	{
		return
			group.prev() ? group.prev()->prevTextInclusive() :
			group() ? group->_exit_this_or_prev_text() :
			nullptr;
	}
	inline Text* Node::nextText() noexcept
	{
		return
			group.next() ? group.next()->nextTextInclusive() :
			group() ? group->_exit_this_or_next_text() :
			nullptr;
	}
	inline Text* Node::prevStop() noexcept
	{
		return
			group.prev() ? group.prev()->_this_or_prev_stop() :
			group() ? group->_exit_this_or_prev_stop() :
			nullptr;
	}
	inline Text* Node::nextStop() noexcept
	{
		return
			group.next() ? group.next()->_this_or_next_stop() :
			group() ? group->_exit_this_or_next_stop() :
			nullptr;
	}
	inline Vector Node::absOffset() const noexcept
	{ 
		Vector result = contentBox().offset;
		for (const Group* n = group(); n; n = n->group())
			result += n->contentBox().offset;
		return result;
	}
	inline Bibliography* Node::bibliography() const noexcept
	{
		return group()->bibliography();
	}

	class Command : public Node
	{
		FontSize _font_size = FontSize::normalsize;
	public:
		string cmd;

		Type type() const noexcept final { return Type::command; }
		Flow flow() const noexcept final { return Flow::line; }

		static auto make() { return Node::make<Command>(); }
		static auto make(string data)
		{
			auto result = make();
			result->cmd = std::move(data);
			return result;
		}

		Node* expand() noexcept final;
		void popArgument(Group& dst) noexcept final { dst.append(group->detach(this)); }
		Node* getArgument() noexcept final { return this; }

		std::optional<string> asEnd() const noexcept final
		{
			if (cmd.size() > 4 && std::string_view(cmd).substr(0,4) == "end ")
				return cmd.substr(4);
			else
				return {};
		}

		Box& updateLayout(Context& con) final;

		void render(tex::Context& con, Vector offset) const final;

		void serialize(std::ostream& out) const override { out << '\\' << cmd; }
	};

	class Text : public Node
	{
		Text* prevTextInclusive() noexcept final { return this; };
		Text* nextTextInclusive() noexcept final { return this; };

	public:
		string text;
		intrusive::list_element<Text, Line> line;

		Font font = { FontType::mono, FontSize::normalsize };
		Mode mode = Mode::text;

		Type type() const noexcept final { return Type::text; }
		Flow flow() const noexcept final { return Flow::line; }

		static auto make() noexcept { return Node::make<Text>(); }
		static auto make(string text) noexcept
		{
			auto result = make();
			result->text = std::move(text);
			return result;
		}
		static auto make(string text, string space) noexcept
		{
			auto result = make();
			result->text = std::move(text);
			result->space_after = std::move(space);
			return result;
		}

		void popArgument(Group& dst) final;
		Node* getArgument() noexcept final { return this; }

		void insertSpace(int offset);
		int insert(int offset, std::string_view text);

		string extract(int offset, int length = -1);

		Box& updateLayout(Context& con) final;
		void render(tex::Context& con, oui::Vector offset) const final;

		void serialize(std::ostream& out) const override { out << text << space_after; }
	};

	class Line : public intrusive::refcount, public intrusive::list<Text, Line, &Text::line>
	{
		intrusive::refcount::ptr<Line> _next;
		intrusive::raw::ptr<Line> _prev;
	public:
		constexpr Line* next() const { return _next.get(); }
		constexpr Line* prev() const { return _prev.get(); }

		friend void push(Owner<Line>& head, Owner<Line> value) noexcept
		{
			if (head)
			{
				head->_prev = value.get();
				value->_next = move(head);
			}
			head = move(value);
		}
	};



	class Par : public Group
	{
	public:
		enum class Type : unsigned char { simple, title, author, section, subsection };
		static constexpr friend std::string_view name(Type t)
		{
			using namespace std::string_view_literals;
			constexpr std::array<const string_view, 5> cmd_name =
			{
				""sv,
				"\\title"sv,
				"\\author"sv,
				"\\section"sv,
				"\\subsection"sv
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
		string terminator;

		Par(string token);

		void enforceRules() noexcept final { _enforce_child_rules(); }

		bool terminatedBy(std::string_view) const noexcept final { return false; }

		void partype(Type t) noexcept;
		constexpr Type partype() noexcept { return _type; }

		bool collect(Paragraph&) noexcept final { return false; }

		Box& updateLayout(Context& con) override;
		void render(Context& con, Vector offset) const final;
		void serialize(std::ostream& out) const final;
	};

	std::vector<nonull<Node*>> interval(Node& a, Node& b);

	Owner<Group> tokenize(std::string_view in);
}
