#include <tex_node.h>
#include <algorithm>

namespace tex
{
	template <>
	std::unique_ptr<Node> BasicNode<Node>::detatch()
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
	template<>
	void BasicNode<Node>::append(std::unique_ptr<Node> child)
	{
		if (child->_parent)
			child->detatch();
		child->_parent = static_cast<Node*>(this);
		child->_prev = _last;
		if (_first)
		{
			_last->_next = std::move(child);
			_last = _last->_next.get();
		}
		else
		{
			_first = std::move(child);
			_last = _first.get();
		}
	}


	Node tokenize(const char *& in, const char * const end, std::string data, Mode mode)
	{
		Node result{ data };

		while (in != end)
		{
			switch (*in)
			{
			case '\\':
			{
				auto& child = result.emplace_back(in, 1);
				++in;
				child.data.push_back(*in);
				for (++in; in != end && isalpha(*in); ++in)
					child.data.push_back(*in);

				if (child.data == "\\begin")
				{
					result.back() = tokenize(in, end, readCurly(in, end), mode);
					continue;
				}
				if (child.data == "\\end")
				{
					if (const auto envname = readCurly(in, end); envname != result.data)
						throw IllFormed("\\begin{" + result.data + "} does not match \\end{" + envname + "}");
					result.back().detatch();
					return result;
				}
				continue;
			}
			case '%':
			{
				auto& tok = result.emplace_back(in, 1).data;
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
				result.emplace_back(tokenize(in, end, "curly", mode));
				continue;
			case '}':
				++in;
				return result;
			case '$':
				++in;
				if (result.data == "math")
					return result;
				else if (mode == Mode::math)
					throw IllFormed("improperly balanced group or environment in math mode");
				result.emplace_back(tokenize(in, end, "math", Mode::math));
				continue;
			default:
			{
				if (*in >= 0 && *in <= ' ')
				{
					auto& tok = result.emplace_back(in, 1).data;
					for (++in; in != end && *in >= 0 && *in <= ' '; ++in)
						tok.push_back(*in);
				}
				else
				{
					auto& tok = result.emplace_back(in, 1).data;
					for (++in; in != end && isregular(*in); ++in)
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
		switch (type())
		{
		case Type::space:
			if (count(data, '\n') >= 2)
				return false;
			out.push_back(this);
			return true;
		case Type::group:
			if (data == "document")
				return false;
			for (auto&& e : *this)
				e._collect_line(con, out);
			return true;
		default:
			out.push_back(this);
			return true;
		}
	}
	Node::Layout Node::updateLayout(Context & con, FontType fonttype, float width)
	{
		font = FontType::sans;
		switch (type())
		{
		case Type::space:
			font = fonttype;
			if (count(data, '\n') >= 2)
				return { { 0,0 }, oui::topLeft, Flow::vertical };
			return
			{
				{ ceil(con.font(font)->height()*0.25f), float(con.font(font)->height()) },
				oui::topLeft,
				Flow::none
			};
		case Type::text:
		case Type::comment:
			font = fonttype;
		case Type::command:
			return { { con.font(font)->offset(data), float(con.font(font)->height()) } };
		case Type::group:
		{
			oui::Point pen;
			Layout result;

			font = fonttype;

			if (data == "document")
			{
				font = FontType::roman;
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
					_layout_line(line, pen, con, width);
					if (it == end())  break;
				}
				const auto l = it->updateLayout(con, font, width);
				it->box = l.align(oui::topLeft(pen).size({ width, 0 })).size(l.size);
				pen.y += l.size.y;
			}
			result.size.x = width;
			result.size.y = pen.y;
			return result;
		}
		default:
			throw std::logic_error("unknown tex::Type");
		}
	}
	void Node::_layout_line(std::vector<tex::Node *> &line, oui::Point &pen, tex::Context & con, float width)
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
			const auto l = (*lit)->updateLayout(con, font, width);
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
	float Node::_align_line(const float line_top, const IT first, const IT last)
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