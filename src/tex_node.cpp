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

	std::vector<Node*> Node::parents()
	{
		std::vector<Node*> result;
		for (auto p = this; p; p = p->group())
			result.push_back(p);
		return result;
	}

	std::vector<Node*> interval(Node& a, Node& b)
	{
		auto ap = a.parents();
		auto bp = b.parents();

		std::vector<Node*> result;
		if (&a == &b)
		{
			result.push_back(&a);
			return result;
		}

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
		{
			Expects(s);
			result.push_back(s);
		}
		// then add nodes from b branch
		for (++ib; ib != bp.rend(); ++ib)
			for (auto s = &(*ib)->group->front(); s != *ib; s = s->group.next())
			{
				Expects(s);
				result.push_back(s);
			}
		result.push_back(bp.front()); // including b

		return result;
	}



	void Node::change() noexcept
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

	Space * Text::insertSpace(int offset)
	{
		if (offset > text.size())
			offset = text.size();
		const auto offset_size = narrow<size_t>(offset);
		insertAfterThis(Text::make(text.substr(offset_size)));
		text.resize(offset_size);
		return insertAfterThis(Space::make());

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
		{
			auto result = Group::make("%");
			auto next_newline = in.find_first_of("\r\n");
			if (next_newline >= in.size())
				result->tokenize(in, mode);
			else
			{
				const auto ender = in[next_newline];
 				const auto after_ender = in.size() > (next_newline + 1) ? in[next_newline + 1] : 0;
				if ((after_ender == '\n' || after_ender == '\r') && after_ender != ender)
					next_newline += 1;
				auto rest_of_line = in.substr(1, next_newline);
				result->tokenize(rest_of_line, mode);
				in.remove_prefix(next_newline + 1);
			}
			return result;
		}
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
			if (auto sub = tokenize_single(in, *this, mode, OnEnd::match))
				append(move(sub));
			else break;
	}



	void Node::popArgument(Group & dst)
	{
		const auto n = group.next();
		dst.append(detachFromGroup());
		tryPopArgument(n, dst);
	}
	void Text::popArgument(Group & dst)
	{
		if (text.empty())
		{
			Expects(group.next() != nullptr);
			return void(group.next()->popArgument(dst));
		}
		const auto frontlen = utf8len(text.front());
		if (text.size() == frontlen)
		{
			dst.append(detachFromGroup());
			return;
		}

		dst.append(Text::make(text.substr(0, frontlen)));
		text.erase(0, frontlen);
	}


	Node* Group::expand()
	{
		for (auto child = &front(); child != nullptr; child = child->group.next())
			child = child->expand();

		return this;
	}

	void Group::enforceRules()
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
