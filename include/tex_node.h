#pragma once

#include "tex_context.h"
#include "express.h"

namespace tex
{
	class Group;
	class Command;
	class Text;

	class Paragraph;
	class InputReader;

	
	class Node : public intrusive::refcount
	{
		friend class Group;

		bool _changed = true;
	protected:
		virtual Text* _this_or_prev_text() noexcept { return prevText(); }
		virtual Text* _this_or_next_text() noexcept { return nextText(); }

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

		Owner<Node> replaceWith(Owner<Node> p);
		Owner<Node> detachFromGroup();
		void removeFromGroup();
		template <class T> T* insertBeforeThis(Owner<T> p);
		template <class T> T* insertAfterThis(Owner<T> p);

		std::vector<Node*> parents();

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
		void widen(float w) { _box.after += w; }

		Node() = default;
		virtual ~Node() = default;
		Node(Node&&) = delete;
		Node(const Node&) = delete;

		Node& operator=(Node&&) = delete;
		Node& operator=(const Node&) = delete;

		constexpr bool changed() const { return _changed; }
		void change() noexcept;
		virtual void commit() noexcept { _changed = false; }


		virtual Node* expand() { return this; }
		virtual void popArgument(Group& dst);
		virtual Node* getArgument() { return group.next()->getArgument(); }

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
		virtual Box& updateLayout(Context& con) = 0;
		virtual void render(Context& con, Vector offset) const = 0;

		virtual void serialize(std::ostream& out) const = 0;
		void serialize(std::ostream&& out) { serialize(out); }
	};
	inline bool text(const Node& n) { return n.type() == Node::Type::text; }
	inline bool text(const Node* n) { return n && text(*n); }
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

	class Group : public Node, public intrusive::list<Node, Group, &Node::group>
	{
		friend class Node;

		Text* _this_or_prev_text() noexcept override { return !empty() ? rbegin()->_this_or_prev_text() : prevText(); }
		Text* _this_or_next_text() noexcept override { return !empty() ?  begin()->_this_or_next_text() : nextText(); }
		virtual Text* _exit_this_or_next_text() noexcept { return nextText(); }
		virtual Text* _exit_this_or_prev_text() noexcept { return prevText(); }

		Text* _this_or_prev_stop() noexcept override { return !empty() ? rbegin()->_this_or_prev_stop() : prevStop(); }
		Text* _this_or_next_stop() noexcept override { return !empty() ?  begin()->_this_or_next_stop() : nextStop(); }
		virtual Text* _exit_this_or_next_stop() noexcept { return nextStop(); }
		virtual Text* _exit_this_or_prev_stop() noexcept { return prevStop(); }

	protected:
		void _enforce_child_rules() { for (auto&& e : *this) e.enforceRules(); }
	public:
		Group() = default;

		void commit() noexcept final;

		Type type() const noexcept final { return Type::group; }
		Flow flow() const noexcept override { return Flow::line; }


		static Owner<Group> make(string name);

		virtual void tokenize(InputReader& in, Mode mode);
		virtual bool terminatedBy(std::string_view token) const = 0;

		Node* expand() override;
		void popArgument(Group& dst) final { dst.append(group->detach(this)); }
		Node* getArgument() noexcept final { return this; }

		void enforceRules() override;

		bool contains(Node* n) const
		{
			for (; n != nullptr; n = n->group())
				if (n == this)
					return true;
			return false;
		}

		Node& front() const { return *begin(); }
		Node& back()  const { return *rbegin(); }

		bool collect(Paragraph& out) override;
		Box& updateLayout(Context& con) override;
		void render(Context& con, Vector offset) const override;

		void serialize(std::ostream& out) const override;
		void serialize(std::ostream&& out) { serialize(out); }
	};

	inline Owner<Node> Node::replaceWith(Owner<Node> p) 
	{ 
		insertBeforeThis(move(p));
		return detachFromGroup();
	}

	inline Owner<Node> Node::detachFromGroup() { return group->detach(this); }
	inline void Node::removeFromGroup() { group->remove(this); }
	template<class T>
	inline T * Node::insertBeforeThis(Owner<T> p) { return group->insert_before(this, move(p)); }
	template<class T>
	inline T * Node::insertAfterThis(Owner<T> p) { return group->insert_after(this, move(p)); }
	inline Text* Node::prevText() noexcept
	{
		return
			group.prev() ? group.prev()->_this_or_prev_text() :
			group() ? group->_exit_this_or_prev_text() :
			nullptr;
	}
	inline Text* Node::nextText() noexcept
	{
		return
			group.next() ? group.next()->_this_or_next_text() :
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
	inline Vector Node::absOffset() const
	{ 
		Vector result = contentBox().offset;
		for (const Group* n = group(); n; n = n->group())
			result += n->contentBox().offset;
		return result;
	}

	class Command : public Node
	{
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

		Node* expand() final;
		void popArgument(Group& dst) final { dst.append(group->detach(this)); }
		Node* getArgument() noexcept final { return this; }

		std::optional<string> asEnd() const final
		{
			if (cmd.size() > 4 && std::string_view(cmd).substr(0,4) == "end ")
				return cmd.substr(4);
			else
				return {};
		}

		Box& updateLayout(Context& con) final;

		void render(tex::Context& con, Vector offset) const final;

		void serialize(std::ostream& out) const override { out << '\\' << cmd; }
		void serialize(std::ostream&& out) { serialize(out); }
	};

	class Text : public Node
	{
		Text* _this_or_prev_text() noexcept final { return this; };
		Text* _this_or_next_text() noexcept final { return this; };

	public:
		string text;
		intrusive::list_element<Text, Line> line;

		Font font = { FontType::mono, FontSize::normalsize };
		Mode mode = Mode::text;

		Type type() const noexcept final { return Type::text; }
		Flow flow() const noexcept final { return Flow::line; }

		static auto make() { return Node::make<Text>(); }
		static auto make(string text)
		{
			auto result = make();
			result->text = std::move(text);
			return result;
		}

		void popArgument(Group& dst) final;
		Node* getArgument() noexcept final { return this; }

		void insertSpace(int offset);
		int insert(int offset, std::string_view text);

		Box& updateLayout(Context& con) final;
		void render(tex::Context& con, oui::Vector offset) const final;

		void serialize(std::ostream& out) const override { out << text; }
		void serialize(std::ostream&& out) { serialize(out); }
	};

	class Line : public intrusive::refcount, public intrusive::list<Text, Line, &Text::line>
	{
		intrusive::refcount::ptr<Line> _next;
		intrusive::raw::ptr<Line> _prev;
	public:
		Line* next() const { return _next.get(); }
		Line* prev() const { return _prev.get(); }

		friend void push(Owner<Line>& head, Owner<Line> value)
		{
			if (head)
			{
				head->_prev = value;
				value->_next = move(head);
			}
			head = move(value);
		}
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
		string terminator;

		Par(string token);

		void enforceRules() final { _enforce_child_rules(); }

		bool terminatedBy(std::string_view) const final { return false; }

		void partype(Type t);
		Type partype() { return _type; }

		bool collect(Paragraph&) final { return false; }

		Box& updateLayout(Context& con) override;
		void render(Context& con, Vector offset) const final;
		void serialize(std::ostream& out) const final;
	};

	std::vector<Node*> interval(Node& a, Node& b);

	Owner<Group> tokenize(std::string_view in);
}
