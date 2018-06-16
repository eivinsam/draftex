#pragma once

#include <string>
#include <stdexcept>

#include <oui_text.h>
#include <oui_window.h>

#include <gsl-lite.hpp>

template <class C, class T>
auto count(C&& c, const T& value) { return std::count(std::begin(c), std::end(c), value); }


inline constexpr int utf8len(unsigned char ch)
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


namespace tex
{
	using gsl::narrow;

	class IllFormed : public std::exception
	{
		std::string _message;
	public:

		IllFormed(std::string message) noexcept : _message(std::move(message)) {  }

		const char* what() const noexcept final { return _message.c_str(); }
	};

	namespace details
	{
		struct VectorHead
		{
			int size = 0;
			int capacity = 0;

			constexpr VectorHead(int capacity) : capacity(capacity) { }
		};
	}

	enum class Mode : char { text, math };
	enum class Flow : char { none, line, vertical };
	enum class FontType : char { mono, sans, roman };


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

		void reset(oui::Window& window) noexcept
		{
			_w = &window;
		}

		gsl::not_null<oui::Font*> font(FontType f)
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

		oui::Window& window() const noexcept { return *_w; }
	};


	inline constexpr bool isregular(char ch)
	{
		switch (ch)
		{
		case '\\': case '%': case '{': case '}': case '$': return false;
		default:
			return ch < 0 || ch > ' ';
		}
	}


}
