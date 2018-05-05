#include <tex_node.h>
#include <algorithm>

namespace tex
{
	Node* Node::insertBefore(Owner<Node> sibling)
	{
		sibling->_parent = _parent;
		sibling->_prev = _prev;
		auto& new_owner = _prev ? _prev->_next : _parent->_first;
		sibling->_next = std::move(new_owner);
		_prev = (new_owner = std::move(sibling)).get();
		return _prev;
	}
	Owner<Node> Node::detach()
	{
		if (!_parent)
			throw std::logic_error("trying to detach loose child");
		auto result = std::move(this == _parent->_first.get() ?
			_parent->_first : _prev->_next);
		if (_next)
			_next->_prev = _prev;
		else
			_parent->_last = _prev;
		_prev->_next = std::move(_next);
		_prev = nullptr;
		_parent = nullptr;
		return result;
	}
	Node& Group::append(Owner<Node> child)
	{
		if (child->_parent)
			child->detach();
		child->_parent = this;
		child->_prev = _last;
		auto& new_owner = !_first ? _first : _last->_next;
		new_owner = std::move(child);
		_last = new_owner.get();
		return *_last;
	}

	std::string parseArg(const char*& in, const char* const end)
	{
		std::string result;
		if (*in == ' ')
		{
			++in;
			switch (*in)
			{
			case '\\':
				result.push_back(*in);
				++in;
				result.push_back(*in);
				for (; isalpha(*in); ++in)
					result.push_back(*in);
				++in;
				return result;
			case '%': case '{': case '$':
				throw IllFormed(std::string("unsupported character sequence ' ") + *in + "' found while looking for argument");
			default:
				if (*in >= 0 && *in <= ' ')
					throw IllFormed("multiple whitespace before argument not supported");
				result.push_back(*in); 
				++in;
				return result;
			}
		}
	}


	Owner<Group> tokenize(const char *& in, const char * const end, std::string data, Mode mode)
	{
		auto result = Group::create(data);

		while (in != end)
		{
			switch (*in)
			{
			case '\\':
			{
				std::string cmd;
				++in;
				cmd.push_back(*in);
				for (++in; in != end && isalpha(*in); ++in)
					cmd.push_back(*in);

				//auto& child = result->append(Command::make());

				if (cmd == "begin")
				{
					result->append(tokenize(in, end, readCurly(in, end), mode));
					continue;
				}
				if (cmd == "end")
				{
					if (const auto envname = readCurly(in, end); envname != result->data)
						throw IllFormed("\\begin{" + result->data + "} does not match \\end{" + envname + "}");
					return result;
				}
				//if (cmd == "frac")
				//{
				//	continue;
				//}
				result->append(Command::make()).data = std::move(cmd);
				continue;
			}
			case '%':
			{
				auto& tok = result->append(Comment::make()).data;
				for (++in; in != end && *in != '\n' && *in != '\r'; ++in)
					tok.push_back(*in);
				if (in != end)
				{
					tok.push_back(*in);
					++in;
					if (in != end &&
						((in[-1] == '\n' && in[0] == '\r') ||
						(in[-1] == '\r' && in[0] == '\n')))
					{
						tok.push_back(*in);
						++in;
					}
				}
				continue;
			}
			case '{':
				++in;
				result->append(tokenize(in, end, "curly", mode));
				continue;
			case '}':
				++in;
				return result;
			case '$':
				++in;
				if (result->data == "math")
					return result;
				else if (mode == Mode::math)
					throw IllFormed("improperly balanced group or environment in math mode");
				result->append(tokenize(in, end, "math", Mode::math));
				continue;
			default:
			{
				if (*in >= 0 && *in <= ' ')
				{
					auto& tok = result->append(Space::make()).data;
					for (; in != end && *in >= 0 && *in <= ' '; ++in)
						tok.push_back(*in);
				}
				else
				{
					auto& tok = result->append(Text::make()).data;
					for (; in != end && isregular(*in); ++in)
						tok.push_back(*in);
				}
				continue;
			}
			}
		}
		return result;
	}

	std::string readCurly(const char *& in, const char * const end)
	{
		if (in == end || *in != '{')
			throw IllFormed("expected '{'");
		std::string result;
		for (++in; in != end && *in != '}'; ++in)
			result.push_back(*in);
		if (in == end)
			throw IllFormed("no matching '}'");
		++in;
		return result;
	}

	bool Node::_collect_line(Context & con, std::vector<Node*>& out)
	{
		out.push_back(this);
		return true;
	}
	bool Group::_collect_line(Context & con, std::vector<Node*>& out)
	{
		if (data == "document")
			return false;
		for (auto&& e : *this)
			e._collect_line(con, out);
		return true;
	}
	bool Space::_collect_line(Context & con, std::vector<Node*>& out)
	{
		if (count(data, '\n') >= 2)
			return false;
		out.push_back(this);
		return true;
	}
	Node* Node::insertSpace(int offset)
	{
		if (offset == 0)
		{
			return (_prev && _prev->isSpace()) ?
				nullptr : insertBefore(Space::make());
		}
		else
			return insertAfter(Space::make());
	}
	Node * Text::insertSpace(int offset)
	{
		if (offset <= 0)
		{
			return (!prev() || !prev()->isSpace()) ?
				insertBefore(Space::make()) : nullptr;
		}
		if (offset >= narrow<int>(data.size()))
		{
			return (!next() || !next()->isSpace()) ?
				insertAfter(Space::make()) : nullptr;
		}
		const auto offset_size = narrow<size_t>(offset);
		insertAfter(Text::make(std::string_view(data).substr(offset_size)));
		data.resize(offset_size);
		return insertAfter(Space::make());

	}
	void Node::insert(int offset, std::string_view text)
	{
		if (text.empty())
			return;
		if (offset == 0)
		{
			if (_prev && _prev->isText())
				_prev->data.append(text.data(), text.size());
			else
				insertBefore(Text::make(text));
		}
		else
		{
			insertAfter(Text::make(text));
		}
	}
	Node::Layout Node::updateLayout(Context & con, FontType fonttype, float width)
	{
		auto font = con.font(fonttype);
		return { { font->offset(data), float(font->height()) } };
	}
	Node::Layout Text::updateLayout(Context & con, FontType fonttype, float width)
	{
		font = fonttype;
		auto font = con.font(fonttype);
		return { { font->offset(data), float(font->height()) } };
	}
	Node::Layout Group::updateLayout(Context & con, FontType fonttype, float width)
	{
		oui::Point pen;
		Layout result;

		//font = fonttype;

		if (data == "document")
		{
			fonttype = FontType::roman;
			width = std::min(width, con.font(fonttype)->height() * 24.0f);
			result.flow = Flow::vertical;
			result.align = oui::topCenter;
		}

		std::vector<Node*> line;

		for (auto it = begin(); it != end(); ++it)
		{
			line.clear();
			if (it->_collect_line(con, line))
			{
				++it;
				while (it != end() && it->_collect_line(con, line))
					++it;
				_layout_line(line, pen, fonttype, con, width);
				if (it == end())  break;
			}
			const auto l = it->updateLayout(con, fonttype, width);
			it->box = l.align(oui::topLeft(pen).size({ width, 0 })).size(l.size);
			pen.y += l.size.y;
		}
		result.size.x = width;
		result.size.y = pen.y;
		return result;
	}
	Node::Layout Space::updateLayout(Context & con, FontType fonttype, float width)
	{
		if (count(data, '\n') >= 2)
			return { { 0,0 }, oui::topLeft, Flow::vertical };
		auto font = con.font(fonttype);
		return
		{
			{ ceil(font->height()*0.25f), float(font->height()) },
			oui::topLeft,
			Flow::none
		};
	}

	void Group::_layout_line(std::vector<tex::Node *> &line, oui::Point &pen, FontType fonttype, 
		tex::Context & con, float width)
	{
		auto line_first = line.begin();
		while (line_first != line.end() && (*line_first)->isSpace())
		{
			(*line_first)->box = oui::topLeft(pen).size({ 0,0 });
			++line_first;
		}
		int space_count = 0;
		for (auto lit = line_first; lit != line.end(); ++lit)
		{
			const auto l = (*lit)->updateLayout(con, fonttype, width);
			if (pen.x + l.size.x < width)
			{
				if ((*lit)->isSpace())
					++space_count;
				(*lit)->box = l.align(pen).size(l.size);
				pen.x = (*lit)->box.max.x;
				continue;
			}
			if ((*lit)->isSpace())
				++space_count;
			// end of line reached
			if (space_count > 0)
			{
				// try to reach back before previous space
				while (!(*lit)->isSpace())
					--lit;
				while ((*lit)->isSpace())
				{
					--lit;
					--space_count;
				}
			}
			float left = width - (*lit)->box.max.x;
			++lit;
			float shift = 0;
			for (auto it = line_first; it != lit; ++it)
			{
				Node& e = **it;
				e.box.min.x += shift;
				if (e.isSpace())
				{
					const auto incr = floor(left / space_count);
					shift += incr;
					left -= incr;
					--space_count;
				}
				e.box.max.x += shift;
			}
			pen.x = 0;
			pen.y += _align_line(pen.y, line_first, lit);
			while (lit != line.end() && (*lit)->isSpace())
			{
				(*lit)->box = oui::topLeft(pen).size({ 0,0 });
				++lit;
			}
			line_first = lit;
			--lit;
		}
		pen.x = 0;
		pen.y += _align_line(pen.y, line_first, line.end());
	}
	template<class IT>
	float Group::_align_line(const float line_top, const IT first, const IT last)
	{
		float line_min = line_top;
		float line_max = line_top;
		for (auto it = first; it != last; ++it)
		{
			Node& e = **it;
			line_min = std::min(line_min, e.box.min.y);
			line_max = std::max(line_max, e.box.max.y);
		}
		const float adjust_y = line_min - line_top;
		for (auto it = first; it != last; ++it)
		{
			Node& e = **it;
			e.box.min.y += adjust_y;
			e.box.max.y += adjust_y;
		}
		return line_max - line_min;
	}

}