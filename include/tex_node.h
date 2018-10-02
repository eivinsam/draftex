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
		virtual const Text* _this_or_next_stop() const noexcept { return nextStop(); }
		virtual const Text* _this_or_prev_stop() const noexcept { return prevStop(); }

		template <class T, class... Args>
		static auto make(Args&&... args) { return intrusive::refcount::make<T>(std::forward<Args>(args)...); }

		mutable Box _box;
	public:
		using string = SmallString;

		intrusive::list_element<Node, Group> group;

		enum class Type : char { text, group, command };

		nonull<Owner<Node>> replaceWith(nonull<Owner<Node>> p) noexcept;
		nonull<Owner<Node>> detachFromGroup() noexcept;
		void removeFromGroup() noexcept;
		template <class T> nonull<T*> insertBeforeThis(nonull<Owner<T>> p) noexcept;
		template <class T> nonull<T*> insertAfterThis(nonull<Owner<T>> p)  noexcept;
		template <class T> nonull<T*> insertBeforeThis(Owner<T> p) noexcept{ return insertBeforeThis(nonull{ move(p) }); }
		template <class T> nonull<T*> insertAfterThis(Owner<T> p)  noexcept{ return insertAfterThis(nonull{ move(p) }); }

		// This node and all parents
		std::vector<nonull<const Node*>> parents() const;

		constexpr void layoutOffset(Vector offset) const noexcept { _box.offset = offset; }
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
		constexpr void widen(float w) const noexcept { _box.after += w; }

		Node() = default;
		virtual ~Node() = default;
		Node(Node&&) = delete;
		Node(const Node&) = delete;

		Node& operator=(Node&&) = delete;
		Node& operator=(const Node&) = delete;

		constexpr bool changed() const { return _changed; }
		void markChange() noexcept;
		virtual void commit() noexcept { _changed = false; }

		virtual bool apply(const std::function<bool(Node&)>& f) { return f(*this); }

		virtual Node* expand() { return this; }
		virtual void popArgument(Group& dst);
		virtual const Node* getArgument() const { return group.next()->getArgument(); }

		virtual void enforceRules() noexcept { };


		virtual Type type() const noexcept = 0;
		virtual Flow flow() const noexcept = 0;

		virtual std::optional<string> asEnd() const noexcept { return {}; }

		virtual const Text* prevTextInclusive() const noexcept { return prevText(); }
		virtual const Text* nextTextInclusive() const noexcept { return nextText(); }
		Text* prevTextInclusive() noexcept { return intrusive::as_mutable(std::as_const(*this).prevTextInclusive()); }
		Text* nextTextInclusive() noexcept { return intrusive::as_mutable(std::as_const(*this).nextTextInclusive()); }
		const Text* prevText() const noexcept;
		const Text* nextText() const noexcept;
		Text* prevText() noexcept { return intrusive::as_mutable(std::as_const(*this).prevText()); }
		Text* nextText() noexcept { return intrusive::as_mutable(std::as_const(*this).nextText()); }

		const Text* prevStop() const noexcept;
		const Text* nextStop() const noexcept;

		virtual const Bibliography* bibliography() const noexcept;

		virtual bool collect(Paragraph& out) const;
		virtual Box& updateLayout(Context& con) const = 0;
		virtual void render(Context& con, Vector offset) const = 0;

		virtual void serialize(std::ostream& out) const = 0;
	};
	inline bool text(const Node& n) noexcept { return n.type() == Node::Type::text; }
	inline bool text(const Node* n) noexcept { return n && text(*n); }
	inline bool nullOrText(const Node* n) noexcept { return !n || text(*n); }
	template <class T> inline bool space(const Owner<T>& n) { return n && space(*n); }
	template <class T> inline bool text(const Owner<T>& n) { return n && text(*n); }
	template <class T> inline bool nullOrSpace(const Owner<T>& n) { return !n || space(*n); }
	template <class T> inline bool nullOrText(const Owner<T>& n) { return !n || text(*n); }

	inline void tryPopArgument(Node* next, Group& dst)
	{
		if (next == nullptr)
			throw IllFormed("end of group reached while looking for command argument");
		next->popArgument(dst);
	}

	class Group : public Node, public intrusive::list<Node, Group, &Node::group>
	{
		friend class Node;

		virtual const Text* _exit_this_or_next_text() const noexcept { return nextText(); }
		virtual const Text* _exit_this_or_prev_text() const noexcept { return prevText(); }

		const Text* _this_or_prev_stop() const noexcept override { return !empty() ? rbegin()->_this_or_prev_stop() : prevStop(); }
		const Text* _this_or_next_stop() const noexcept override { return !empty() ?  begin()->_this_or_next_stop() : nextStop(); }
		virtual const Text* _exit_this_or_next_stop() const noexcept { return nextStop(); }
		virtual const Text* _exit_this_or_prev_stop() const noexcept { return prevStop(); }

	protected:
		void _enforce_child_rules() noexcept { for (auto&& e : *this) e.enforceRules(); }
	public:
		Group() = default;

		void commit() noexcept final;

		bool apply(const std::function<bool(Node&)>& f) override;

		Type type() const noexcept final { return Type::group; }
		Flow flow() const noexcept override { return Flow::line; }

		const Text* prevTextInclusive() const noexcept override { return !empty() ? rbegin()->prevTextInclusive() : prevText(); }
		const Text* nextTextInclusive() const noexcept override { return !empty() ?  begin()->nextTextInclusive() : nextText(); }

		static Owner<Group> make(string_view name) noexcept { return make(Word(name)); }
		static Owner<Group> make(const Word& name) noexcept;

		virtual void tokenize(InputReader& in, Mode mode);
		void tokenizeText(string_view);
		virtual bool terminatedBy(std::string_view token) const noexcept = 0;

#pragma warning(push)
#pragma warning(disable: 26440)
		Node* expand() override;
		void popArgument(Group& dst) final { dst.append(group->detach(this)); }
#pragma warning(pop)
		const Node* getArgument() const noexcept final { return this; }

		void enforceRules() noexcept override;

		constexpr bool contains(const Node* n) const
		{
			for (; n != nullptr; n = n->group())
				if (n == this)
					return true;
			return false;
		}

		constexpr Node& front() { return *begin(); }
		constexpr Node& back()  { return *rbegin(); }
		constexpr const Node& front() const { return *begin(); }
		constexpr const Node& back()  const { return *rbegin(); }

		bool collect(Paragraph& out) const override;
		Box& updateLayout(Context& con) const override;
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
	inline const Text* Node::prevText() const noexcept
	{
		return
			group.prev() ? group.prev()->prevTextInclusive() :
			group() ? group->_exit_this_or_prev_text() :
			nullptr;
	}
	inline const Text* Node::nextText() const noexcept
	{
		return
			group.next() ? group.next()->nextTextInclusive() :
			group() ? group->_exit_this_or_next_text() :
			nullptr;
	}
	inline const Text* Node::prevStop() const noexcept
	{
		return
			group.prev() ? group.prev()->_this_or_prev_stop() :
			group() ? group->_exit_this_or_prev_stop() :
			nullptr;
	}
	inline const Text* Node::nextStop() const noexcept
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
	inline const Bibliography* Node::bibliography() const noexcept
	{
		return group()->bibliography();
	}

	class Command : public Node
	{
		mutable FontSize _font_size = FontSize::normalsize;
	public:
		Word cmd;

		Type type() const noexcept final { return Type::command; }
		Flow flow() const noexcept final { return Flow::line; }

		static auto make() { return Node::make<Command>(); }
		static auto make(Word w)
		{
			auto result = make();
			result->cmd = std::move(w);
			return result;
		}

		Node* expand() noexcept final;
		void popArgument(Group& dst) noexcept final { dst.append(group->detach(this)); }
		const Node* getArgument() const noexcept final { return this; }

		std::optional<string> asEnd() const noexcept final;

		Box& updateLayout(Context& con) const final;

		void render(tex::Context& con, Vector offset) const final;

		void serialize(std::ostream& out) const override { out << '\\' << cmd; }
	};

	class Text : public Node
	{
		const Text* prevTextInclusive() const noexcept final { return this; };
		const Text* nextTextInclusive() const noexcept final { return this; };

		Word _text;
	public:
		intrusive::list_element<const Text, Line> line;

		decltype(auto) text()  { return _text.text(); }
		decltype(auto) space() { return _text.space(); }
		decltype(auto) text()  const { return _text.text(); }
		decltype(auto) space() const { return _text.space(); }

		const Word& word() const { return _text; }

		mutable Font font = { FontType::mono, FontSize::normalsize };
		mutable Mode mode = Mode::text;

		Type type() const noexcept final { return Type::text; }
		Flow flow() const noexcept final { return Flow::line; }

		static auto make() noexcept { return Node::make<Text>(); }
		static auto make(string text) noexcept
		{
			auto result = make();
			result->_text = Word{ std::move(text) };
			return result;
		}

		void popArgument(Group& dst) final;
		const Node* getArgument() const noexcept final { return this; }

		Box& updateLayout(Context& con) const final;
		void render(tex::Context& con, oui::Vector offset) const final;

		void serialize(std::ostream& out) const override { out << _text; }
	};

	class Line : public intrusive::refcount, public intrusive::list<const Text, Line, &Text::line>
	{
		intrusive::refcount::ptr<Line> _next;
		intrusive::raw::ptr<Line> _prev;
	public:
		constexpr Line* next() const { return _next.get(); }
		constexpr Line* prev() const { return _prev.get(); }

		friend void push(Owner<Line>& head, Owner<Line> value) noexcept;
	};



	class Par : public Group
	{
	public:
		enum class Type : unsigned char { simple, title, author, section, subsection };
		friend std::string_view name(Type t);
	private:
		static constexpr auto _code(Type t) { return static_cast<unsigned char>(t); }

		Type _type;
		mutable Font _font;
		mutable string _pretitle;
	protected:
		mutable float _parindent = 0;
	public:
		string terminator;

		Par(const Word& token);
		Par(string_view s) : Par(Word(s)) { }

		void enforceRules() noexcept final { _enforce_child_rules(); }

		bool terminatedBy(std::string_view) const noexcept final { return false; }

		void partype(Type t) noexcept;
		constexpr Type partype() noexcept { return _type; }

		bool collect(Paragraph&) const noexcept final { return false; }

		Box& updateLayout(Context& con) const override;
		void render(Context& con, Vector offset) const final;
		void serialize(std::ostream& out) const final;
	};

	// all nodes from a to b inclusive
	std::vector<nonull<const Node*>> interval(const Node& a, const Node& b);

	Owner<Group> tokenize(std::string_view in);

	bool refreshCites(Node&);
}
