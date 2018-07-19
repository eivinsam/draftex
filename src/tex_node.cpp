#include <tex_node.h>
#include <algorithm>
#include <numeric>
#include <cassert>

using std::move;
using std::string;
using std::string_view;
using std::make_unique;

inline string_view view(const string& s) noexcept { return string_view{ s }; }

namespace tex
{
	using uchar = unsigned char;

	static inline int popCodepoint(std::string_view& text)
	{
		if (text.empty())
			return 0;
		int code = 0;
		const uchar x = uchar(text.front()); text.remove_prefix(1);
		uchar remaining = 0;
		switch (x >> 4)
		{
		case 0b1111:
			code = x & 0x7;
			remaining = 3;
			break;
		case 0b1110:
			code = x & 0xf;
			remaining = 2;
			break;
		case 0b1100:
		case 0b1101:
			code = x & 0x1f;
			remaining = 1;
			break;
		default:
			if (x & 0x80)
				throw std::runtime_error("unepected continuation byte");
			return x;
		}
		for (; remaining > 0; --remaining)
		{
			if (text.empty())
				return 0;
			const uchar y = uchar(text.front()); text.remove_prefix(1);
			if (y >> 6 != 2)
				throw std::runtime_error("continuation byte expected");
			code = (code << 6) | (y & 0x3f);
		}
		return code;
	}


	constexpr char pop_front(string_view& in)
	{
		const char result = in.front();
		in.remove_prefix(1);
		return result;
	}

	namespace align = oui::align;

	void Node::_insert_before(Owner<Node> sibling)
	{
		Expects(sibling->parent == nullptr);
		auto& sibling_ref = *sibling;
		sibling->parent = parent;
		sibling->prev = prev;
		auto& new_owner = prev ? prev->_owning_next() : parent->_first;
		sibling->_owning_next() = std::move(new_owner);
		new_owner = std::move(sibling);
		prev = &sibling_ref;
		prev->change();
	}
	void Group::_append(Owner<Node> child)
	{
		Expects(child->parent == nullptr);
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
		
		change();
		auto result = move(this == parent->_first.get() ?
			parent->_first : prev->next.owning());
		if (next)
			next->prev = prev;
		else
			parent->_last = prev;
		prev->next = move(next.owning());
		prev = nullptr;
		parent = nullptr;
		return result;
	}

	Space * Text::insertSpace(int offset)
	{
		if (offset > int_size(data))
			offset = int_size(data);
		const auto offset_size = narrow<size_t>(offset);
		insertAfter(Text::make(string_view(data).substr(offset_size)));
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



	template <char... Values>
	constexpr bool is(char ch)
	{
		return ((ch == Values) || ...);
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
		auto result = Group::make(move(data));

		if (result->data == "document")
		{
			auto par = Group::make("par");
			while (!in.empty())
				if (auto sub = tokenize_single(in, result->data, mode, OnEnd::match))
				{
					if (sub->isSpace() && count(sub->data, '\n') >= 2)
					{
						result->append(move(par));
						par = Group::make("par");
					}
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

	string readCurly(string_view& in)
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



	void Node::popArgument(Group & dst)
	{
		Node* const n = next;
		dst.append(detach());
		tryPopArgument(n, dst);
	}
	void Text::popArgument(Group & dst)
	{
		assert(!data.empty());

		const auto frontlen = narrow<size_t>(utf8len(data.front()));
		if (data.size() == frontlen)
		{
			dst.append(detach());
			return;
		}

		dst.append(Text::make(view(data).substr(0, frontlen)));
		data.erase(0, frontlen);
	}

	static string_view read_optional_text(string_view data)
	{
		assert(!data.empty());
		if (data.front() != '[')
			return {};

		if (const auto found = data.find_first_of(']', 1); found != data.npos)
			return data.substr(0, found + 1);

		throw IllFormed("could not find end of optional argument (only non-space text supported)");
	}
	static Owner<Node> read_optional(Node* next)
	{
		if (!next || !next->isText())
			return {};

		assert(!next->data.empty());

		if (next->data.front() != '[')
			return {};

		const auto opt = read_optional_text(next->data);

		if (opt.empty())
			return {};

		if (opt.size() == next->data.size())
			return next->detach();
	
		auto result = Text::make(opt);
		next->data.erase(0, opt.size());
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

		if (!result)
			return this;

		const auto raw_result = result.get();
		const auto forget_self = replace(move(result));
		return raw_result;
	}


}