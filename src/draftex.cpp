#include <vector>
#include <string>
#include <string_view>
#include <variant>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include <oui_window.h>
#include <oui_text.h>

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

template <class C, class T>
auto count(C&& c, const T& value) { return std::count(std::begin(c), std::end(c), value); }

template <class To, class From>
To narrow(From from)
{
	auto to = static_cast<To>(from);
	if (static_cast<From>(to) != to)
		throw std::runtime_error("narrowing falied");
	if constexpr (std::is_signed_v<To> != std::is_signed_v<From>)
		if (to < To{} != from < From{})
			throw std::runtime_error("narrowing failed");
	return to;
}

namespace tex
{
	class IllFormed : public std::exception
	{
		std::string _message;
	public:

		IllFormed(std::string message) : _message(std::move(message)) {  }

		const char* what() const final { return _message.c_str(); }
	};

	namespace details
	{
		struct VectorHead
		{
			int size = 0;
			int capacity = 0;

			VectorHead(int capacity) : capacity(capacity) { }
		};
	}

	enum class Mode : char { text, math };
	enum class Type : char { text, space, command, comment, group };
	enum class Flow : char { none, line, vertical };
	enum class FontType : char { mono, sans, roman };

	template <class T>
	class BasicNode
	{
		using Owner = std::unique_ptr<T>;
		T* _parent = nullptr;
		Owner _next;
		T* _prev = nullptr;
		Owner _first;
		T* _last = nullptr;
	public:
		constexpr BasicNode() = default;
		BasicNode(BasicNode&& b) :
			_parent(b._parent),
			_next(std::move(b._next)), _prev(b._prev),
			_first(std::move(b._first)), _last(b._last)
		{
			for (auto&& e : *this)
				e._parent = static_cast<T*>(this);
		}

		BasicNode& operator=(BasicNode&& b)
		{
			_parent = b._parent;
			_next = std::move(b._next);
			_prev = b._prev;
			_first = std::move(b._first);
			_last = b._last;

			for (auto&& e : *this)
				e._parent = static_cast<T*>(this);

			return *this;
		}

		T* next() const { return _next.get(); }
		T* prev() const { return _prev; }

		Owner detatch()
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

		void append(Owner child)
		{
			if (child->_parent)
				child->detatch();
			child->_parent = static_cast<T*>(this);
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

		template <class... Args>
		T& emplace_back(Args&&... args)
		{
			append(std::make_unique<T>(std::forward<Args>(args)...));
			return back();
		}

		T* parent() const { return _parent; }
		bool empty() const { return !_first; }

		T& front() { return *_first; }
		T& back() { return *_last; }
		const T& front() const { return *_first; }
		const T& back() const { return *_last; }

		template <class ValueType>
		class Iterator
		{
			friend class BasicNode<T>;
			ValueType* _it;
			Iterator(ValueType* it) : _it(it) { }
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = void;
			using value_type = ValueType;
			using reference  = ValueType&;
			using pointer    = ValueType*;

			Iterator& operator++() { _it = _it->_next.get(); return *this; }

			reference operator*() const { return *_it; }
			pointer operator->() const { return _it; }

			bool operator==(const Iterator& other) const { return _it == other._it; }
			bool operator!=(const Iterator& other) const { return _it != other._it; }
		};

		using iterator = Iterator<T>;
		using const_iterator = Iterator<const T>;

		      iterator begin()       { return { _first.get() }; }
		const_iterator begin() const { return { _first.get() }; }
		constexpr       iterator end()       { return { nullptr }; }
		constexpr const_iterator end() const { return { nullptr }; }
	};

	class Context
	{
		oui::Window* _w;
		oui::Font mono;
		oui::Font sans;
		oui::Font roman;
	public:

		Context(oui::Window& window) : _w(&window), 
			mono{ "fonts/LinLibertine_Mah.ttf", int(20 * _w->dpiFactor()) },
			sans{ "fonts/LinBiolinum_Rah.ttf", int(20 * _w->dpiFactor()) },
			roman{ "fonts/LinLibertine_Rah.ttf", int(20 * _w->dpiFactor()) } {}

		void reset(oui::Window& window)
		{
			_w = &window;
		}

		oui::Font* font(FontType f)
		{
			switch (f)
			{
			case FontType::mono: return &mono;
			case FontType::sans: return &sans;
			case FontType::roman: return &roman;
			default:
				throw std::logic_error("unknown tex::FontType");
			}
		}

		oui::Window& window() const { return *_w; }
	};

	class Node : public BasicNode<Node>
	{
		template <class IT>
		float _align_line(const float line_top, const IT first, const IT last);
		bool _collect_line(Context& con, std::vector<Node*>& out);
		void _layout_line(std::vector<tex::Node *> &line, oui::Point &pen, tex::Context & con, float width);
	public:
		std::string data;
		oui::Rectangle box;
		FontType font = FontType::mono;

		Node(std::string data) : data(data) { }
		Node(const char* str, size_t len) : data(str, len) { }

		Type type() const
		{
			if (!empty())
				return Type::group;
			switch (data.front())
			{
			case '\\': return Type::command;
			case '%': return Type::comment;
			default:
				return data.front() < 0 || data.front() > ' ' ? 
					Type::text : Type::space;
			}
		}

		bool isText() const { return type() == Type::text; }
		bool isSpace() const { return type() == Type::space; }

		struct Layout
		{
			oui::Vector size;
			oui::Align align = oui::topLeft;
			Flow flow = Flow::line;
		};

		Layout updateLayout(Context& con, FontType fonttype, float width);
	private:
	};

	inline bool isregular(char ch)
	{
		switch (ch)
		{
		case '\\': case '%': case '{': case '}': case '$': return false;
		default:
			return ch < 0 || ch > ' ';
		}
	}

	std::string readCurly(const char*& in, const char* const end)
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

	Node tokenize(const char*& in, const char* const end, std::string data, Mode mode)
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
						throw IllFormed("\\begin{"+result.data+"} does not match \\end{"+envname+"}");
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
	Node tokenize(std::string_view in) 
	{
		auto first = in.data();
		return tokenize(first, first + in.size(), "root", Mode::text);
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
					else  continue;
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


std::string readFile(const std::string& filename)
{
	std::string result;
	std::ifstream file(filename, std::ios::binary);
	file.seekg(0, std::ios::end);
	if (!file)
		return result;
	auto size = static_cast<size_t>(file.tellg());
	result.resize(size);
	file.seekg(0, std::ios::beg);
	file.read(result.data(), result.size());
	return result;
}

void render(const oui::Vector& offset, const tex::Node& node, tex::Context& con)
{
	using tex::Type;
	switch (node.type())
	{
	case Type::text:
	case Type::command:
	case Type::comment:
		con.font(node.font)->drawLine(node.box.min + offset, node.data, oui::colors::black);
		return;
	case Type::space:
		return;
	case Type::group:
	{
		if (node.data == "curly")
		{
			oui::fill(node.box + offset, oui::Color{ 0.8f, 0.7f, 0.0f, 0.3f });
		}
		const auto suboffset = offset + node.box.min - oui::Point{ 0,0 };
		for (auto&& e : node)
			render(suboffset, e, con);
		return;
	}
	default:
		throw std::logic_error("unknown tex::Type");
	}
}

int utf8len(unsigned char ch)
{
	switch (ch >> 4)
	{
	case 0xf: return 4;
	case 0xe: return 3;
	case 0xd: case 0xc: return 2;
	default:
		return (ch >> 7) ^ 1;
	}
}

struct Caret
{
	tex::Node* node = nullptr;
	int offset = 0;

	void render(tex::Context& con)
	{
		if (!con.window().focus())
			return;

		auto pos = node->box.min;
		pos.x += con.font(node->font)->offset(std::string_view(node->data).substr(0, offset)) -1;
		for (auto p = node->parent(); p != nullptr; p = p->parent())
			pos = pos + (p->box.min - oui::origo);

		oui::fill(oui::topLeft(pos).size({ 2.0f, float(con.font(node->font)->height()) }), oui::colors::black);
	}

	int repairOffset(int off)
	{
		while (off > 0 && utf8len(node->data[off]) == 0)
			--off;
		return off;
	}

	void next()
	{
		const auto next_node = [&]
		{
		};
		if (!node)
			return;

		if (!node->isText() || 
			(offset += utf8len(node->data[offset])) >= narrow<int>(node->data.size()))
		{
			if (node->next())
			{
				node = node->next();
				offset = 0;
				return;
			}
			offset = narrow<int>(node->data.size());
		}
	}
	void prev()
	{
		if (!node)
			return;
		if (offset > 0)
		{
			--offset;
			if (node->isText())
				offset = repairOffset(offset);
			return;
		}
		if (node->prev())
		{
			node = node->prev();
			offset = node->isText() ? narrow<int>(node->data.size()-1) : 0;
			return;
		}
		offset = 0;
	}

	void findPlace(tex::Context& con, const float target)
	{
		if (!node->isText())
		{
			offset = 0;
			if (target - node->box.min.x > node->box.max.x - target)
				next();
			return;
		}
		const auto font = con.font(node->font);
		const auto text = std::string_view(node->data);
		auto prev_x = node->box.min.x;
		for (int i = 0, len = 1; size_t(i) < text.size(); i += len)
		{
			len = utf8len(node->data[i]);
			const auto x = prev_x + font->offset(text.substr(i, len));
			if (x >= target)
			{
				offset = x - target > target - prev_x ? i : i + len;
				return;
			}
			prev_x = x;
		}
		offset = text.size();
	}

	void up(tex::Context& con)
	{
		if (!node)
			return;

		const auto target = node->box.min.x 
			+ con.font(node->font)->offset(std::string_view(node->data).substr(0, offset));

		for (auto n = node->prev(); n != nullptr; n = n->prev())
			if (n->box.min.x <= target && target < n->box.max.x)
			{
				node = n;
				findPlace(con, target);
				return;
			}
	}
	void down(tex::Context& con)
	{
		if (!node)
			return;

		const auto target = node->box.min.x
			+ con.font(node->font)->offset(std::string_view(node->data).substr(0, offset));

		for (auto n = node->next(); n != nullptr; n = n->next())
			if (n->box.min.x <= target && target < n->box.max.x)
			{
				node = n;
				findPlace(con, target);
				return;
			}
	}
	void eraseNext()
	{
		const auto handle_empty = [this]
		{
			if (node->next())
			{
				node = node->next();
				node->prev()->detatch();
				return;
			}
			if (node->prev())
			{
				prev();
				node->next()->detatch();
				return;
			}
		};

		if (!node->isText())
		{
			if (offset > 0)
				return;
			handle_empty();
			return;
		}
		node->data.erase(offset, utf8len(node->data[offset]));
		if (node->data.empty())
		{
			handle_empty();
		}
	}
	void erasePrev()
	{
		auto old_node = node;
		auto old_off = offset;
		prev();
		if (old_off != offset || old_node != node)
			eraseNext();
	}
};

int main()
{
	oui::Window window{ { "draftex", 1280, 720 } };

	auto tokens = tex::tokenize(readFile("test.tex"));

	tex::Context context(window);

	Caret caret;
	for (auto&& e : tokens)
		if (e.data == "document")
		{
			for (auto&& de : e)
				if (de.type() == tex::Type::text)
				{
					caret.node = &de;
					break;
				}
			break;
		}

	bool layout_dirty = true;
	window.resize = [&](auto&&) { layout_dirty = true; };

	oui::input.keydown = [&](auto key)
	{
		using oui::Key;
		switch (key)
		{
		case Key::right: caret.next(); break;
		case Key::left: caret.prev(); break;
		case Key::up: caret.up(context); break;
		case Key::down: caret.down(context); break;
		case Key::backspace: caret.erasePrev(); layout_dirty = true; break;
		case Key::del:       caret.eraseNext(); layout_dirty = true; break;
		default: 
			return;
		}
		window.redraw();
	};
	oui::input.character = [&](auto ch)
	{
		if (ch < ' ')
			return;
		if (ch == ' ')
			return;

		char utf8[] = { 0,0,0,0,0 };
		if (ch < 0x80)
			utf8[0] = char(ch);
		else if (ch < 0x800)
		{
			utf8[0] = 0xc0 | (ch >> 6);
			utf8[1] = 0x80 | (ch & 0x3f);
		}
		else
		{
			utf8[0] = 0xe0 | ((ch >> 12) & 0x0f);
			utf8[1] = 0x80 | ((ch >>  6) & 0x3f);
			utf8[2] = 0x80 | ( ch        & 0x3f);
		}
		caret.node->data.insert(caret.offset, utf8);
		caret.offset += strlen(utf8);
		layout_dirty = true;
		window.redraw();
		return;
	};

	while (window.update())
	{
		window.clear(oui::colors::white);
		context.reset(window);
		if (std::exchange(layout_dirty, false))
		{
			tokens.updateLayout(context, tex::FontType::sans, window.area().width());
		}

		render({ 0,0 }, tokens, context);

		caret.render(context);
	}


	return 0;
}
