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

	inline std::string_view trimBack(const std::string& str) 
	{
		return std::string_view(str).substr(0, str.find_first_of(" \t\n\r"));
	}

	template <class T>
	class Vector
	{
		using Head = details::VectorHead;
		std::unique_ptr<Head> _data = nullptr;

		std::unique_ptr<Head> _alloc(int new_capacity)
		{
			return std::unique_ptr<Head>{ new (new char[sizeof(Head) + sizeof(T)*new_capacity]) Head(new_capacity) };
		}
		      T* _first_unchecked()       { return reinterpret_cast<      T*>(_data.get() + 1); }
		const T* _first_unchecked() const { return reinterpret_cast<const T*>(_data.get() + 1); }

		void _ensure_space()
		{
			if (!_data)
				_data = _alloc(4);
			else if (_data->size == _data->capacity)
				reserve(_data->capacity + std::max(1, _data->capacity >> 1)); // increase by 50%
		}
	public:

		int size() const { return _data ? _data->size : 0; }
		bool empty() const { return size() == 0; }

		void reserve(int new_capacity)
		{
			if (!_data)
			{
				_data = _alloc(new_capacity);
				return;
			}
			if (_data->capacity >= new_capacity)
				return;

			Vector temp;
			temp.reserve(new_capacity);
			for (auto&& e : *this)
				temp.push_back(std::move(e));
			std::swap(*this, temp);
		}

		T& push_back(const T& e)
		{
			_ensure_space();
			++_data->size;
			return *new (_first_unchecked() + (_data->size - 1)) T(e);
		}
		T& push_back(T&& e)
		{
			_ensure_space();
			++_data->size;
			return *new (_first_unchecked() + (_data->size - 1)) T(std::move(e));
		}

		template <class... Args>
		T& emplace_back(Args&&... args)
		{
			_ensure_space();
			++_data->size;
			return *new (_first_unchecked() + (_data->size - 1)) T(std::forward<Args>(args)...);
		}

		void pop_back()
		{
			if (empty())
				return;
			back().~T();
			--_data->size;
		}

		      T* begin()       { return !_data ? nullptr : _first_unchecked(); }
		const T* begin() const { return !_data ? nullptr : _first_unchecked(); }
		      T* end()       { return !_data ? nullptr : _first_unchecked() + _data->size; }
		const T* end() const { return !_data ? nullptr : _first_unchecked() + _data->size; }

		      T& front()       { return _first_unchecked()[0]; }
		const T& front() const { return _first_unchecked()[0]; }
		      T& back()       { return _first_unchecked()[_data->size-1]; }
		const T& back() const { return _first_unchecked()[_data->size-1]; }
	};

	enum class Type : char { text, space, command, comment, group };
	enum class Flow : char { none, line, vertical };
	enum class Font : char { mono, sans, roman };

	class Context
	{
		oui::Font mono;
		oui::Font sans;
		oui::Font roman;
	public:
		oui::Font* current_font;

		Context(const oui::Window& window) : 
			mono{ "fonts/LinLibertine_Mah.ttf", int(20*window.dpiFactor()) },
			sans{ "fonts/LinBiolinum_Rah.ttf", int(20*window.dpiFactor()) }, 
			roman{ "fonts/LinLibertine_Rah.ttf", int(20*window.dpiFactor()) }, 
			current_font{ &roman } { }

		void reset(const oui::Window& window)
		{
			current_font = &roman;
		}

		oui::Font* font(Font f)
		{
			switch (f)
			{
			case Font::mono: return &mono;
			case Font::sans: return &sans;
			case Font::roman: return &roman;
			default:
				throw std::logic_error("unknown tex::Font");
			}
		}
	};

	struct Node
	{
		std::string data;
		oui::Rectangle box;
		Vector<Node> children;

		Node(std::string data) : data(data) { }
		Node(const char* str, size_t len) : data(str, len) { }

		Type type() const
		{
			if (!children.empty())
				return Type::group;
			switch (data.front())
			{
			case '\\': return Type::command;
			case '%': return Type::comment;
			default:
				return data.front() >= 0 && data.front() <= ' ' ? Type::space : Type::text;
			}
		}


		struct Layout
		{
			oui::Vector size;
			oui::Align align = oui::topLeft;
			Flow flow = Flow::line;
		};

		Layout updateLayout(Context& con, float width)
		{
			switch (type())
			{
			case Type::text: 
			case Type::comment:
			case Type::command:
				return { { con.current_font->offset(trimBack(data)), float(con.current_font->height()) } };
			case Type::space:
				return
				{
					{ 0, 0 },
					oui::topLeft,
					count(data, '\n') >= 2 ? Flow::vertical : Flow::none
				};
			case Type::group:
			{
				oui::Point pen;
				Layout result;


				if (data == "document")
				{
					//con.current_font = con.font(Font::roman);
					width = con.current_font->height() * 20.0f;
					result.flow = Flow::vertical;
					result.align = oui::topCenter;
				}

				float line_height = 0;
				Node* line_first = nullptr;

				for (auto&& e : children)
				{
					const auto l = e.updateLayout(con, width);
					switch (l.flow)
					{
					case Flow::vertical:
						line_first = nullptr;
						pen = { 0, pen.y + line_height };
						result.flow = Flow::vertical;
						line_height = 0;
						e.box = l.align(oui::topLeft(pen).size({ width, 0 })).size(l.size);
						break;
					case Flow::line:
						if (line_first == nullptr && e.type() != Type::space)
							line_first = &e;
						else if (pen.x + l.size.x > width)
						{
							int count = 0;
							for (auto it = line_first; it != &e; ++it)
								if (it->type() != Type::space)
									++count;
							const float incr = (width - pen.x)/(count-1);
							float shift = 0;
							for (auto it = line_first; it != &e; ++it)
							{
								it->box.min.x += shift;
								it->box.max.x += shift;
								if (it->type() != Type::space)
									shift += incr;
							}
							line_first = &e;
							pen = { 0, pen.y + line_height };
							line_height = l.size.y;
						}
						else 
							if (line_height < l.size.y)
								line_height = l.size.y;
						e.box = l.align(pen).size(l.size);
						pen.x = pen.x + l.size.x + con.current_font->height()*0.25f;
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

	Node tokenize(const char*& in, const char* const end, std::string data)
	{
		Node result{ data };

		while (in != end)
		{
			switch (*in)
			{
			case '\\': 
			{
				auto& tok = result.children.emplace_back(in, 1).data;
				++in; 
				tok.push_back(*in);
				for (++in; in != end && isalpha(*in); ++in)
					tok.push_back(*in);
				for (; in != end && (*in == ' ' || *in == '\t'); ++in)
					tok.push_back(*in);

				const auto trimtok = trimBack(tok);
				if (trimtok == "\\begin")
				{
					if (tok != "\\begin")
						throw IllFormed("spaces after \\begin not supported");
					result.children.back() = tokenize(in, end, readCurly(in, end));
					continue;
				}
				if (trimtok == "\\end")
				{
					if (tok != "\\end")
						throw IllFormed("spaces after \\end not supported");
					if (const auto envname = readCurly(in, end); envname != result.data)
						throw IllFormed("\\begin{"+result.data+"} does not match \\end{"+envname+"}");
					result.children.pop_back();
					return result;
				}
				continue;
			}
			case '%':
			{
				auto& tok = result.children.emplace_back(in, 1).data;
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
				result.children.emplace_back(tokenize(in, end, "curly"));
				continue;
			case '}':
				++in;
				return result;
			case '$':
				++in;
				if (result.data == "math")
					return result;
				result.children.emplace_back(tokenize(in, end, "math"));
				continue;
			default:
			{
				if (*in >= 0 && *in <= ' ')
				{
					auto& tok = result.children.emplace_back(in, 1).data;
					for (++in; in != end && *in >= 0 && *in <= ' '; ++in)
						tok.push_back(*in);
				}
				else
				{
					auto tok = result.children.empty() || !result.children.back().children.empty() ?
						nullptr : &result.children.back().data;
					if (tok && *tok == " ")
						tok->assign(in, 1);
					else
						tok = &result.children.emplace_back(in, 1).data;
					for (++in; in != end && isregular(*in); ++in)
						tok->push_back(*in);
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
		return tokenize(first, first + in.size(), "root");
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
		con.current_font->drawLine(node.box.min + offset, tex::trimBack(node.data), oui::colors::black);
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
		for (auto&& e : node.children)
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
		tokens.updateLayout(context, window.area().width());

		render({ 0,0 }, tokens, context);
	}


	return 0;
}
