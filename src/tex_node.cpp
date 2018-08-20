#include "tex_node_internal.h"
#include <algorithm>
#include <numeric>


using std::move;

using oui::utf8len;
using oui::popCodepoint;


namespace tex
{
	using uchar = unsigned char;

	constexpr char pop_front(string_view& in)
	{
		const char result = in.front();
		in.remove_prefix(1);
		return result;
	}

	static string readCurly(string_view& in)
	{
		if (in.empty() || in.front() != '{')
			throw IllFormed("expected '{'");
		in.remove_prefix(1);
		string result;
		while (!in.empty() && in.front() != '}')
			result.push_back(pop_front(in));
		if (in.empty())
			throw IllFormed("no matching '}'");
		in.remove_prefix(1);
		return result;
	}

	namespace align = oui::align;

	void Node::_insert_before(Owner<Node> sibling)
	{
		Expects(sibling != nullptr);
		Expects(sibling->parent == nullptr);

		//if (!text(prev()) && sibling->_needs_text_before(prev()))
		//	_insert_before(Text::make(""));

		auto& sibling_ref = *sibling;
		sibling->parent = parent;
		sibling->prev = prev;
		auto& new_owner = prev ? prev->_owning_next() : parent->_first;
		sibling->_owning_next() = std::move(new_owner);
		new_owner = std::move(sibling);
		prev = &sibling_ref;
		prev->change();

		//if (!text(*prev) && _needs_text_before(prev()))
		//	_insert_before(Text::make(""));
	}
	void Group::_append(Owner<Node> child)
	{
		Expects(child != nullptr);
		Expects(child->parent == nullptr);

		//if (!text(_last) && child->_needs_text_before(_last))
		//	_append(Text::make(""));

		Node& child_ref = *child;
		child->_set_parent(this);
		child->_set_prev(_last);
		auto& new_owner = !_first ? _first : _last->_owning_next();
		new_owner = std::move(child);
		_last = &child_ref;
		_last->change();
	}

	void Node::_insert_after(Owner<Node> sibling)
	{
		next ?
			next->insertBefore(std::move(sibling)) :
			parent->append(std::move(sibling));
	}

	float Node::absLeft() const
	{
		float left = box.left();
		for (const Group* p = parent(); p != nullptr; p = p->parent())
			left += p->box.offset.x;
		return left;
	}
	float Node::absTop() const
	{
		float top = box.top();
		for (const Group* p = parent(); p != nullptr; p = p->parent())
			top += p->box.offset.y;
		return top;
	}
	oui::Rectangle Node::absBox() const
	{
		oui::Rectangle result;
		result.min = box.min();
		for (const Group* p = parent(); p != nullptr; p = p->parent())
			result.min += p->box.offset;
		result.max = result.min + oui::Vector{ box.width(), box.height() };
		return result;
	}

	void Node::change() noexcept
	{
		for (Node* n = this; n != nullptr; n = n->parent())
			n->_changed = true;
	}
	void Group::commit() noexcept
	{
		Node::commit();
		for (auto&& child : *this)
			child.commit();
	}

	Owner<Node> Node::detach()
	{
		if (!parent)
			throw std::logic_error("trying to detach loose child");

		// do removing
		change();
		auto result = move(this == parent->_first.get() ?
			parent->_first : prev->next.owning());

		if (next)
			next->prev = prev;
		else
			parent->_last = prev();

		if (prev)
		{
			prev->next = move(next.owning());
			prev = nullptr;
		}
		else
			parent->_first = move(next.owning());

		parent = nullptr;

		return result;
	}

	Space * Text::insertSpace(int offset)
	{
		if (offset > text.size())
			offset = text.size();
		const auto offset_size = narrow<size_t>(offset);
		insertAfter(Text::make(text.substr(offset_size)));
		text.resize(offset_size);
		return insertAfter(Space::make());

	}

	int Text::insert(int offset, std::string_view new_text)
	{
		change();
		text.insert(offset, new_text);
		return int_size(new_text);
	}





	Owner<Node> tokenize_single(string_view& in, Group& parent, Mode mode, OnEnd on_end)
	{
		switch (in.front())
		{
		case '\\':
		{
			string cmd;
			in.remove_prefix(1);
			if (in.empty())
				throw IllFormed("end of input after '\\'");
			cmd.push_back(pop_front(in));
			for (; !in.empty() && isalpha(in.front()); in.remove_prefix(1))
				cmd.push_back(in.front());

			if (cmd == "begin")
			{
				return tokenize(in, readCurly(in), mode);
			}
			if (cmd == "end")
			{
				auto end_of = readCurly(in);
				switch (on_end)
				{
				case OnEnd::pass: return Command::make("end " + end_of);
				case OnEnd::fail: throw IllFormed(std::string("unexpected \\end{" + end_of + "}"));
				case OnEnd::match:
					if (parent.terminatedBy(end_of))
						return nullptr;
					throw IllFormed(std::string("unexpected \\end{" + end_of + "}"));
				default:
					throw std::logic_error("unknown OnEnd value");
				}
			}
			return Command::make(move(cmd));
		}
		case '%':
			throw IllFormed("comments not supported");
		case '{':
			in.remove_prefix(1);
			return tokenize(in, "curly", mode);
		case '}':
			in.remove_prefix(1);
			if (parent.terminatedBy("}"))
				return nullptr;
			throw IllFormed("unexpected }");
		case '$':
			in.remove_prefix(1);
			if (parent.terminatedBy("$"))
				return nullptr;
			else if (mode == Mode::math)
				throw IllFormed("improperly balanced group or environment in math mode");
			return tokenize(in, "math", Mode::math);
		default:
			if (in.front() >= 0 && in.front() <= ' ')
			{
				auto result = Space::make();
				while (!in.empty() && in.front() >= 0 && in.front() <= ' ')
					result->space.push_back(pop_front(in));
				return result;
			}
			else
			{
				auto result = Text::make();
				while (!in.empty() && isregular(in.front()))
					result->text.push_back(pop_front(in));
				return result;
			}
		}
	}

	void Group::tokenize(string_view & in, Mode mode)
	{
		while (!in.empty())
			if (auto sub = tokenize_single(in, *this, mode, OnEnd::fail))
				append(move(sub));
			else break;
	}



	void Node::popArgument(Group & dst)
	{
		const auto n = next();
		dst.append(detach());
		tryPopArgument(n, dst);
	}
	void Text::popArgument(Group & dst)
	{
		if (text.empty())
		{
			Expects(next != nullptr);
			return void(next->popArgument(dst));
		}
		const auto frontlen = utf8len(text.front());
		if (text.size() == frontlen)
		{
			dst.append(detach());
			return;
		}

		dst.append(Text::make(text.substr(0, frontlen)));
		text.erase(0, frontlen);
	}

	static string read_optional_text(string_view data)
	{
		Expects(!data.empty());
		if (data.front() != '[')
			return {};

		if (const auto found = data.find_first_of(']', 1); found != data.npos)
			return string(data.substr(0, found + 1));

		throw IllFormed("could not find end of optional argument (only non-space text supported)");
	}
	static Owner<Node> read_optional(Node* next)
	{
		auto text = as<Text>(next);
		if (!text || text->text.empty())
			return {};

		if (text->text.front() != '[')
			return {};

		auto opt = read_optional_text(text->text);

		if (opt.empty())
			return {};

		if (opt.size() == text->text.size())
			return next->detach();

		auto result = Text::make(move(opt));
		text->text.erase(0, result->text.size());
		return result;
	}

	Node* Group::expand()
	{
		for (auto child = _first.get(); child != nullptr; child = child->next())
			child = child->expand();

		return this;
	}

	void Group::enforceRules()
	{
		if (parent)
		{
			if (!text(prev()))
				insertBefore(Text::make());
			if (!text(next()))
				insertAfter(Text::make());
		}
		_enforce_child_rules();
	}

	using CommandExpander = Owner<Group>(*)(Command*);

	// pops mandatory, optional and then mandatory argument, no expansion
	Owner<Group> expand_aoa(Command* src)
	{
		Owner<Group> result = Group::make(src->cmd);
		tryPopArgument(src->next(), *result);
		if (auto opt = read_optional(src->next()))
			result->append(move(opt));
		tryPopArgument(src->next(), *result);
		return result;
	}
	// pops command plus and optional and a mandatory argument, no expansion
	Owner<Group> expand_coa(Command* src)
	{
		auto result = Group::make(src->cmd);
		result->append(Command::make(std::move(src->cmd)));
		if (auto opt = read_optional(src->next()))
			result->append(move(opt));
		tryPopArgument(src->next(), *result);
		return result;
	}

	// pops one argument and expands it
	Owner<Group> expand_A(Command* src)
	{
		auto result = Group::make(src->cmd);
		tryPopArgument(src->next(), *result); result->back().expand();
		return result;
	}
	// pops two arguments, expanding both
	Owner<Group> expand_AA(Command* src)
	{
		auto result = Group::make(src->cmd);
		tryPopArgument(src->next(), *result); result->back().expand();
		tryPopArgument(src->next(), *result); result->back().expand();
		return result;
	}


	Node * Command::expand()
	{
		static constexpr frozen::unordered_map<string_view, CommandExpander, 8> cases =
		{
			{ "newcommand", &expand_aoa },
			{ "usepackage", &expand_coa },
			{ "documentclass", &expand_coa },
			{ "frac", &expand_AA },
			{ "title", &expand_A },
			{ "author", &expand_A },
			{ "section", &expand_A },
			{ "subsection", &expand_A }
		};

		auto cmd_case = cases.find(cmd);
		if (cmd_case == cases.end())
			return this;

		auto result = cmd_case->second(this);

		const auto raw_result = result.get();
		const auto forget_self = replace(move(result));
		return raw_result;
	}

	void Group::serialize(std::ostream & out) const
	{
		for (auto&& e : *this)
			e.serialize(out);
	}

	Par::Par(string token)
	{
		static constexpr frozen::unordered_map<string_view, Type, 5> type_lookup = 
		{
		{ "par", Type::simple },
		{ "title", Type::title },
		{ "author", Type::author },
		{ "section", Type::section },
		{ "subsection", Type::subsection }
		};
		auto type = type_lookup.find(token);
		if (type == type_lookup.end())
			throw IllFormed("unknown par type");
		_type = type->second;
	}
}
