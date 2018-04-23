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
	enum class Type : char { text, command, comment, group };
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
		oui::Font mono;
		oui::Font sans;
		oui::Font roman;
	public:

		Context(const oui::Window& window) :
			mono{ "fonts/LinLibertine_Mah.ttf", int(20 * window.dpiFactor()) },
			sans{ "fonts/LinBiolinum_Rah.ttf", int(20 * window.dpiFactor()) },
			roman{ "fonts/LinLibertine_Rah.ttf", int(20 * window.dpiFactor()) } {}

		void reset(const oui::Window& window)
		{
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
	};

	class Node : public BasicNode<Node>
	{
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
				return Type::text;
			}
		}
		std::string_view text() const { return std::string_view(data).substr(0, data.find_last_not_of(" \t\r\n")+1); }
		std::string_view space() const { return std::string_view(data).substr(std::min(data.size(), data.find_last_not_of(" \t\r\n")+1)); }

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
				for (; in != end && (*in == ' ' || *in == '\t'); ++in)
					child.data.push_back(*in);

				const auto trimtok = child.text();
				if (trimtok == "\\begin")
				{
					if (child.data != "\\begin")
						throw IllFormed("spaces after \\begin not supported");
					result.back() = tokenize(in, end, readCurly(in, end), mode);
					continue;
				}
				if (trimtok == "\\end")
				{
					if (child.data != "\\end")
						throw IllFormed("spaces after \\end not supported");
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
					for (; in != end && (*in == ' ' || *in == '\t'); ++in)
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
	Node::Layout Node::updateLayout(Context & con, FontType fonttype, float width)
	{
		font = FontType::sans;
		switch (type())
		{
		case Type::text:
			if (text() == "")
				return
				{
					{ 0, 0 },
					oui::topLeft,
					count(data, '\n') >= 2 ? Flow::vertical : Flow::none
				};
		case Type::comment:
			font = fonttype;
		case Type::command:
			return { { con.font(font)->offset(text()), float(con.font(font)->height()) } };
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

			float line_height = 0;
			auto line_first = end();

			for (auto it = begin(); it != end(); ++it)
			{
				auto&& e = *it;
				const auto l = e.updateLayout(con, font, width);
				switch (l.flow)
				{
				case Flow::vertical:
					line_first = end();
					pen = { 0, pen.y + line_height };
					result.flow = Flow::vertical;
					line_height = 0;
					e.box = l.align(oui::topLeft(pen).size({ width, 0 })).size(l.size);
					break;
				case Flow::line:
					if (line_first == end() && e.text() != "")
						line_first = it;
					else if (pen.x + l.size.x > width)
					{
						int count = 0;
						for (auto jt = line_first; jt != it; ++jt)
							if (jt->text() != "")
								++count;
						const float incr = (width - pen.x) / (count - 1);
						float shift = 0;
						for (auto jt = line_first; jt != it; ++jt)
						{
							jt->box.min.x += shift;
							jt->box.max.x += shift;
							if (jt->text() != "")
								shift += incr;
						}
						line_first = it;
						pen = { 0, pen.y + line_height };
						line_height = l.size.y;
					}
					else
						if (line_height < l.size.y)
							line_height = l.size.y;
					e.box = l.align(pen).size(l.size);
					pen.x = pen.x + l.size.x + con.font(fonttype)->height()*0.25f;
					break;
				}
				switch (l.flow)
				{
				case Flow::vertical:
					break;
				case Flow::line:
					break;
				}
				if (result.size.x < e.box.max.x)
					result.size.x = e.box.max.x;
				if (result.size.y < e.box.max.y)
					result.size.y = e.box.max.y;
			}
			if (result.flow == Flow::vertical)
				result.size.x = width;
			return result;
		}
		default:
			throw std::logic_error("unknown tex::Type");
		}
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
		if (node.text() == "")
			return;
	case Type::command:
	case Type::comment:
		con.font(node.font)->drawLine(node.box.min + offset, node.text(), oui::colors::black);
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


int main()
{
	oui::Window window{ { "draftex", 1280, 720 } };

	auto tokens = tex::tokenize(readFile("test.tex"));



	tex::Context context(window);

	while (window.update(oui::Window::Messages::wait))
	{
		window.clear(oui::colors::white);
		context.reset(window);
		tokens.updateLayout(context, tex::FontType::sans, window.area().width());

		render({ 0,0 }, tokens, context);
	}


	return 0;
}
