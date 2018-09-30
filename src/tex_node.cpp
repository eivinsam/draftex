#include "tex_node_internal.h"
#include <algorithm>
#include <numeric>


using std::move;
using std::as_const;

using oui::utf8len;
using oui::popCodepoint;


namespace tex
{
	Owner<Node> InputReader::_tokenize_single(const Group& parent, Mode mode, OnEnd on_end)
	{
		switch (**this)
		{
		case '\\':
		{
			Word cmd;
			skip();
			if (!*this)
				throw IllFormed("end of input after '\\'");
			if (isalpha(**this))
			{
				while (*this && isalpha(**this))
					cmd.text().push_back(pop());
				while (*this && isSpace(**this))
					cmd.space().push_back(pop());
			}
			else
				cmd.text().push_back(pop());

			if (view(cmd.text()) == "begin")
			{
				return tokenize_group(readCurly(), mode);
			}
			if (view(cmd.text()) == "end")
			{
				auto end_of = readCurly();
				switch (on_end)
				{
				case OnEnd::pass: return Command::make(Word("end " + end_of));
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
		{
			auto result = Group::make("%");
			const auto next_newline = in.find_first_of("\r\n");
			if (next_newline >= in.size())
				result->tokenize(*this, mode);
			else
			{
				auto after_space = next_newline+1;
				if (after_space < in.size() &&
					((in[after_space] == '\n' && in[next_newline] == '\r') ||
					 (in[after_space] == '\r' && in[next_newline] == '\n')))
					++after_space;
				after_space = in.find_first_not_of(" \t", after_space);
				auto rest_of_line = InputReader{ in.substr(1, after_space-1) };
				result->tokenize(rest_of_line, mode);
				in.remove_prefix(after_space);
			}
			return result;
		}
		case '{':
			skip();
			return tokenize_group("curly", mode);
		case '}':
			skip();
			if (parent.terminatedBy("}"))
				return nullptr;
			throw IllFormed("unexpected }");
		case '$':
			skip();
			if (parent.terminatedBy("$"))
				return nullptr;
			else if (mode == Mode::math)
				throw IllFormed("improperly balanced group or environment in math mode");
			return tokenize_group("math", Mode::math);
		default:
			auto result = Text::make();
			while (*this && isregular(**this))
				result->text().push_back(pop());
			while (*this && isSpace(**this))
				result->space().push_back(pop());
			return result;
		}
	}

	string InputReader::readCurly()
	{
		if (!*this || **this != '{')
			throw IllFormed("expected '{'");
		skip();
		string result;
		while (*this && **this != '}')
			result.push_back(pop());
		if (!*this)
			throw IllFormed("no matching '}'");
		skip();
		return result;
	}


	using uchar = unsigned char;


	namespace align = oui::align;

	std::vector<nonull<const Node*>> Node::parents() const
	{
		std::vector<nonull<const Node*>> result;
		for (auto p = this; p; p = p->group())
			result.push_back(p);
		return result;
	}


	std::vector<nonull<const Node*>> interval(const Node& a, const Node& b)
	{
		std::vector<nonull<const Node*>> result;
		if (&a == &b)
		{
			result.push_back(&a);
			return result;
		}

		auto ap = a.parents();
		auto bp = b.parents();


		auto ia = ap.rbegin();
		auto ib = bp.rbegin();
		for (; *ia == *ib; ++ia, ++ib)
			if (ia == ap.rend() || ib == bp.rend())
				return result;

		for (auto sa = (*ia)->group.next(), sb = (*ib)->group.next(); sb && sa != *ib; sa = sa->group.next(), sb = sb->group.next())
		{
			// check if sa is later than sb
			if (!sa || sb == *ia)
			{
				std::swap(ap, bp);
				std::swap(ia, ib);
				break;
			}
		}

		// add nodes from a branch
		result.push_back(ap.front()); // including a
		for (auto ja = --ap.rend(); ja != ia; --ja)
			for (auto s = (*ja)->group.next(); s; s = s->group.next())
				result.push_back(s);
		// add nodes between ia and ib
		for (auto s = (*ia)->group.next(); s != *ib; s = s->group.next())
			result.push_back(s);
		// then add nodes from b branch
		for (++ib; ib != bp.rend(); ++ib)
			for (auto s = &(*ib)->group->front(); s != *ib; s = s->group.next())
				result.push_back(s);
		result.push_back(bp.front()); // including b

		return result;
	}

	Owner<Group> tokenize(std::string_view text_in)
	{
		auto in = InputReader{ text_in };
		auto result = Group::make("root");
		result->tokenize(in, Mode::text);
		return result;
	}



	void Node::markChange() noexcept
	{
		for (Node* n = this; n != nullptr; n = n->group())
			n->_changed = true;
	}
	void Group::commit() noexcept
	{
		Node::commit();
		for (auto&& child : *this)
			child.commit();
	}

	bool Group::apply(const std::function<bool(Node&)>& f)
	{
		if (f(*this))
		{
			for (auto&& e : *this)
				e.apply(f);
			return true;
		}
		return false;
	}







	void Group::tokenize(InputReader& in, Mode mode)
	{
		while (in)
			if (auto sub = in.tokenize_single(*this, mode, OnEnd::match))
				append(move(sub));
			else break;
	}

	void Group::tokenizeText(string_view text)
	{
		InputReader in{ text };
		tokenize(in, Mode::text);
	}



	void Node::popArgument(Group & dst)
	{
		const auto n = group.next();
		dst.append(detachFromGroup());
		tryPopArgument(n, dst);
	}
	void Text::popArgument(Group & dst)
	{
		if (text().empty())
		{
			auto next = group.next();
			Expects(next != nullptr);
			dst.append(detachFromGroup());
			return void(next->popArgument(dst));
		}
		const auto frontlen = utf8len(text().front());
		if (text().size() == frontlen)
		{
			dst.append(detachFromGroup());
			return;
		}

		dst.append(Text::make(view(text()).substr(0, frontlen)));
		text().erase(0, frontlen);
	}


	Node* Group::expand()
	{
		for (auto child = &front(); child != nullptr; child = child->group.next())
			child = child->expand();

		return this;
	}

	void Group::enforceRules() noexcept
	{
		if (group())
		{
			if (!text(group.prev()))
				insertBeforeThis(Text::make());
			if (!text(group.next()))
				insertAfterThis(Text::make());
		}
		_enforce_child_rules();
	}


	void Group::serialize(std::ostream & out) const
	{
		for (auto&& e : *this)
			e.serialize(out);
	}

	void push(Owner<Line>& head, Owner<Line> value) noexcept
	{
		if (head)
		{
			head->_prev = value.get();
			value->_next = move(head);
		}
		head = move(value);
	}

	std::string_view name(Par::Type t)
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
	Par::Par(const Word& token)
	{
		Expects(token.space().empty());
		static constexpr frozen::unordered_map<string_view, Type, 5> type_lookup = 
		{
		{ "par", Type::simple },
		{ "title", Type::title },
		{ "author", Type::author },
		{ "section", Type::section },
		{ "subsection", Type::subsection }
		};
		auto type = type_lookup.find(token.text());
		if (type == type_lookup.end())
			throw IllFormed("unknown par type");
		_type = type->second;
	}
}
