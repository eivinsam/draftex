#include <vector>
#include <string>
#include <string_view>
#include <variant>
#include <fstream>
#include <algorithm>

#include <oui_window.h>
#include <oui_text.h>

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

namespace tex
{
	namespace details
	{
		struct VectorHead
		{
			int size = 0;
			int capacity = 0;

			VectorHead(int capacity) : capacity(capacity) { }
		};
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

		      T* begin()       { return !_data ? nullptr : _first_unchecked(); }
		const T* begin() const { return !_data ? nullptr : _first_unchecked(); }
		      T* end()       { return !_data ? nullptr : _first_unchecked() + _data->size; }
		const T* end() const { return !_data ? nullptr : _first_unchecked() + _data->size; }

		      T& front()       { return _first_unchecked()[0]; }
		const T& front() const { return _first_unchecked()[0]; }
		      T& back()       { return _first_unchecked()[_data->size-1]; }
		const T& back() const { return _first_unchecked()[_data->size-1]; }
	};

	struct Node
	{
		std::string data;
		Vector<Node> children;

		Node(std::string data) : data(data) { }
		Node(const char* str, size_t len) : data(str, len) { }
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
				continue;
			}
			case '%':
			{
				auto& tok = result.children.emplace_back(in, 1).data;
				for (++in; in != end && *in != '\n' && *in != '\r'; ++in)
					tok.push_back(*in);
				if (in != end)
				{
					++in;
					if (in != end &&
						((in[-1] == '\n' && in[0] == '\r') ||
						(in[-1] == '\r' && in[0] == '\n')))
					{
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

int main()
{
	oui::Window window{ { "draftex", 1280, 720 } };

	oui::Font font("Segoe UI", int(24*window.dpiFactor()));


	auto tokens = tex::tokenize(readFile("test.tex"));

	while (window.update())
	{
		window.clear(oui::colors::white);
		auto area = window.area();

		
		font.drawText(area, "hello", oui::colors::black);
	}


	return 0;
}
