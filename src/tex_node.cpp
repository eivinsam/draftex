#include <tex_node.h>
#include <algorithm>
#include <numeric>
#include <cassert>

using std::move;
using std::string;
using std::string_view;
using std::make_unique;

using oui::utf8len;
using oui::popCodepoint;

inline string_view view(const string& s) noexcept { return string_view{ s }; }

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

	bool Space::_needs_text_before(Node * otherwise) const 
	{ 
		return otherwise != nullptr && 
			otherwise->type() == Type::group && 
			otherwise->data != "curly"; 
	}

	void Node::_insert_before(Owner<Node> sibling)
	{
		Expects(sibling != nullptr);
		Expects(sibling->parent == nullptr);

		if (sibling->_needs_text_before(prev) && !text(prev))
			_insert_before(Text::make(""));

		auto& sibling_ref = *sibling;
		sibling->parent = parent;
		sibling->prev = prev;
		auto& new_owner = prev ? prev->_owning_next() : parent->_first;
		sibling->_owning_next() = std::move(new_owner);
		new_owner = std::move(sibling);
		prev = &sibling_ref;
		prev->change();

		if (_needs_text_before(prev) && !text(*prev))
			_insert_before(Text::make(""));
	}
	void Group::_append(Owner<Node> child)
	{
		Expects(child != nullptr);
		Expects(child->parent == nullptr);

		if (child->_needs_text_before(_last) && !text(_last))
			_append(Text::make(""));

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
		for (const Group* p = parent; p != nullptr; p = p->parent)
			left += p->box.offset.x;
		return left;
	}
	float Node::absTop() const
	{
		float top = box.top();
		for (const Group* p = parent; p != nullptr; p = p->parent)
			top += p->box.offset.y;
		return top;
	}
	oui::Rectangle Node::absBox() const
	{
		oui::Rectangle result;
		result.min = box.min();
		for (const Group* p = parent; p != nullptr; p = p->parent)
			result.min += p->box.offset;
		result.max = result.min + oui::Vector{ box.width(), box.height() };
		return result;
	}

	void Node::change() noexcept
	{
		for (Node* n = this; n != nullptr; n = n->parent)
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
			parent->_last = prev;

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
		if (offset > int_size(data))
			offset = int_size(data);
		const auto offset_size = narrow<size_t>(offset);
		insertAfter(Text::make(data.substr(offset_size)));
		data.resize(offset_size);
		return insertAfter(Space::make());

	}

	int Text::insert(int offset, std::string_view text)
	{
		change();
		const auto uoffset = narrow<size_t>(offset);

		data.insert(uoffset, text.data(), text.size());
		return int_size(text);
	}




	enum class OnEnd : char { pass, match, fail };

	Owner<Node> tokenize_single(string_view& in, string_view env, Mode mode, OnEnd on_end)
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
				case OnEnd::fail: throw IllFormed("unexpected \\end{" + end_of + "}");
				case OnEnd::match:
					if (end_of == env)
						return nullptr;
					throw IllFormed("\\begin{" + string(env) + "} mismatced with \\end{" + end_of + "}");
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
			return nullptr;
		case '$':
			in.remove_prefix(1);
			if (env == "math")
				return nullptr;
			else if (mode == Mode::math)
				throw IllFormed("improperly balanced group or environment in math mode");
			return tokenize(in, "math", Mode::math);
		default:
			if (in.front() >= 0 && in.front() <= ' ')
			{
				auto result = Space::make();
				while (!in.empty() && in.front() >= 0 && in.front() <= ' ')
					result->data.push_back(pop_front(in));
				return result;
			}
			else
			{
				auto result = Text::make();
				while (!in.empty() && isregular(in.front()))
					result->data.push_back(pop_front(in));
				return result;
			}
		}
	}



	Owner<Group> tokenize(string_view& in, string data, Mode mode)
	{
		static constexpr std::string_view headings[] = 
		{
			"title",
			"author",
			"section",
			"subsection"
		};
		auto result = Group::make(move(data));

		if (result->data == "document")
		{
			Owner<Group> par;
			while (!in.empty())
				if (auto sub = tokenize_single(in, result->data, mode, OnEnd::match))
				{
					if (sub->type() == Node::Type::command)
					{
						using namespace xpr;
						if (any.of(headings).are(equal.to(sub->data)))
						{
							if (par) result->append(move(par));
							if (in.front() != '{')
								throw IllFormed("expected { after \\" + sub->data);
							in.remove_prefix(1);
							auto arg = tokenize(in, "curly", mode);
							auto headg = Group::make(sub->data);
							headg->append(move(arg));
							result->append(move(headg));
							continue;
						}
					}
					else if (space(*sub) && count(sub->data, '\n') >= 2)
					{
						if (par) result->append(move(par));
						par = Group::make("par");
					}
					if (!par) par = Group::make("par");
					par->append(move(sub));
				}
				else break;
			result->append(move(par));
			return result;
		}

		while (!in.empty())
			if (auto sub = tokenize_single(in, result->data, mode, OnEnd::fail))
				result->append(move(sub));
			else break;
				
		return result;
	}

	void Node::popArgument(Group & dst)
	{
		Node* const n = next;
		dst.append(detach());
		tryPopArgument(n, dst);
	}
	void Text::popArgument(Group & dst)
	{
		if (data.empty())
		{
			assert(next != nullptr);
			return void(next->popArgument(dst));
		}
		const auto frontlen = narrow<size_t>(utf8len(data.front()));
		if (data.size() == frontlen)
		{
			dst.append(detach());
			return;
		}

		dst.append(Text::make(data.substr(0, frontlen)));
		data.erase(0, frontlen);
	}

	static string read_optional_text(string_view data)
	{
		assert(!data.empty());
		if (data.front() != '[')
			return {};

		if (const auto found = data.find_first_of(']', 1); found != data.npos)
			return string(data.substr(0, found + 1));

		throw IllFormed("could not find end of optional argument (only non-space text supported)");
	}
	static Owner<Node> read_optional(Node* next)
	{
		if (!next || !text(next) || next->data.empty())
			return {};

		if (next->data.front() != '[')
			return {};

		auto opt = read_optional_text(next->data);

		if (opt.empty())
			return {};

		if (opt.size() == next->data.size())
			return next->detach();
	
		auto result = Text::make(move(opt));
		next->data.erase(0, result->data.size());
		return result;
	}

	Node* Group::expand()
	{
		for (auto child = _first.get(); child != nullptr; child = child->next)
			child = child->expand();

		return this;
	}
	Node * Command::expand()
	{
		Owner<Group> result;
		if (data == "newcommand")
		{
			result = Group::make(data);
			tryPopArgument(next, *result);
			if (auto opt = read_optional(next))
				result->append(move(opt));
			tryPopArgument(next, *result);
		}

		if (data == "usepackage" || data == "documentclass")
		{
			result = Group::make(data);
			result->append(Command::make(std::move(data)));
			if (auto opt = read_optional(next))
				result->append(move(opt));
			tryPopArgument(next, *result);
		}

		if (data == "frac")
		{
			result = Group::make(data);
			tryPopArgument(next, *result); result->back().expand();
			tryPopArgument(next, *result); result->back().expand();
		}

		if (data == "section" || data == "subsection")
		{
			result = Group::make(data);
			tryPopArgument(next, *result);
		}

		if (!result)
			return this;

		const auto raw_result = result.get();
		const auto forget_self = replace(move(result));
		return raw_result;
	}

	void Group::serialize(std::ostream & out) const
	{
		if (data == "math")
		{
			out << '$';
			_serialize_children(out);
			out << '$';
			return;
		}
		if (data == "curly")
		{
			out << '{';
			_serialize_children(out);
			out << '}';
			return;
		}

		_serialize_children(out);
	}
	void Command::serialize(std::ostream & out) const
	{
		out << '\\' << data;
	}
	void Space::serialize(std::ostream & out) const
	{
		out << data;
	}
	void Text::serialize(std::ostream & out) const
	{
		out << data;
	}

}