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

	template <class C>
	constexpr int int_size(C&& c) { return narrow<int>(std::size(c)); }

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
	enum class FontType : char { mono, sans, roman, italic };

	enum class FontSize : char 
	{ 
		tiny = -1, // 5.22pt
		scriptsize = 1, // 6.89pt
		footnotesize = 2, // 7.92pt
		small = 3, // 9.09pt
		normalsize = 4, //10.45pt
		large = 5, // 12.00pt
		Large = 6, // 13.78pt
		LARGE = 7, // 18.19pt
		huge = 9, // 20.89pt
		Huge = 10 // 24.00pt
	};
	inline constexpr FontSize shift(FontSize size, char steps = 1) { return static_cast<FontSize>(static_cast<char>(size) + steps); }
	inline constexpr FontSize twice(FontSize size) { return shift(size, 5); }
	inline constexpr FontSize half(FontSize size) { return shift(size, -5); }

	struct Font
	{
		FontType type;
		FontSize size;
	};

	inline float ptsize(FontSize size, float key) 
	{ 
		return key * pow(2.0f, static_cast<char>(size) / 5.0f);
	}

	class Context
	{
		oui::Window* _w;
		oui::VectorFont mono;
		oui::VectorFont sans;
		oui::VectorFont roman;
		oui::VectorFont italic;
	public:
		float keysize = 6;

		Context(oui::Window& window) : _w(&window),
			mono{ "fonts/LinLibertine_Mah.ttf" },
			sans{ "fonts/LinBiolinum_Rah.ttf" },
			roman{ "fonts/LinLibertine_Rah.ttf" },
			italic{ "fonts/LinLibertine_RIah.ttf" } {}

		void reset(oui::Window& window) noexcept
		{
			_w = &window;
		}

		gsl::not_null<oui::VectorFont*> font(FontType f)
		{
			switch (f)
			{
			case FontType::mono: return &mono;
			case FontType::sans: return &sans;
			case FontType::roman: return &roman;
			case FontType::italic: return &italic;
			default:
				throw std::logic_error("unknown tex::FontType");
			}
		}
		auto font(Font f) { return font(f.type); }

		oui::Window& window() const noexcept { return *_w; }

		float ptsize(FontSize size) const { return tex::ptsize(size, keysize); }
		float ptsize(Font f) const { return ptsize(f.size); }
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
