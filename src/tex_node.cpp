#include "tex_node_internal.h"
#include <algorithm>
#include <numeric>


using std::move;

using oui::utf8len;
using oui::popCodepoint;


namespace tex
{
	Owner<Node> InputReader::_tokenize_single(Group& parent, Mode mode, OnEnd on_end)
	{
		switch (**this)
		{
		case '\\':
		{
			string cmd;
			skip();
			if (!*this)
				throw IllFormed("end of input after '\\'");
			cmd.push_back(pop());
			while (*this && isalpha(**this))
				cmd.push_back(pop());

			if (cmd == "begin")
			{
				return tokenize_group(readCurly(), mode);
			}
			if (cmd == "end")
			{
				auto end_of = readCurly();
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
				result->tokenize(*this, mode);
			else
			{
				auto rest_of_line = InputReader{ in.substr(1, next_newline-1) };
				result->tokenize(rest_of_line, mode);
				in.remove_prefix(next_newline);
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
				result->text.push_back(pop());
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

	Owner<Group> tokenize(std::string_view text_in)
	{
		auto in = InputReader{ text_in };
		auto result = Group::make("root");
		result->tokenize(in, Mode::text);
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

	void Text::insertSpace(int offset)
	{
		change();
		if (offset >= text.size())
		{
			if (!space_after.empty())
				insertAfterThis(Text::make())
				->space_after = move(space_after);
			space_after = " ";
		}
		else
		{
			insertAfterThis(Text::make(text.substr(offset)))
				->space_after = move(space_after);
			text.resize(offset);
			space_after = " ";
		}
	}

	int Text::insert(int offset, std::string_view new_text)
	{
		change();
		text.insert(offset, new_text);
		return int_size(new_text);
	}







	void Group::tokenize(InputReader& in, Mode mode)
	{
		while (in)
			if (auto sub = in.tokenize_single(*this, mode, OnEnd::match))
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
