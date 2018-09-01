#pragma once

#include <string>
#include <stdexcept>

#include <oui_text.h>
#include <oui_window.h>

#include "tex_util.h"

namespace tex
{
	using oui::Vector;
	using oui::Point;
	using oui::Rectangle;

	template <class C>
	constexpr int int_size(C&& c) { return narrow<int>(std::size(c)); }

	class IllFormed : public std::exception
	{
		std::string _message;
	public:

		IllFormed(std::string message) noexcept : _message(std::move(message)) {  }

		const char* what() const noexcept final { return _message.c_str(); }
	};

	enum class Mode : char { text, math };
	enum class Flow : char { none, line, vertical };
	enum class FontType : char { mono, sans, roman, italic, bold };

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

	class Box
	{
	public:
		oui::Vector offset;
		float before;
		float above;
		float after;
		float below;

		constexpr float left() const { return offset.x - before; }
		constexpr float top() const { return offset.y - above; }
		constexpr float right() const { return offset.x + after; }
		constexpr float bottom() const { return offset.y + below; }

		constexpr oui::Point min() const { return { left(), top() }; }
		constexpr oui::Point max() const { return { right(),  bottom() }; }

		constexpr float width() const { return before + after; }
		constexpr float height() const { return above + below; }

		constexpr void width(float w, oui::Align a) { before = w * a.c; after = w - before; }
		constexpr void height(float h, oui::Align a) { above = h * a.c; below = h - above; }
	};

	class Float;

	class Context
	{
		oui::Window* _w;
		oui::VectorFont mono;
		oui::VectorFont sans;
		oui::VectorFont roman;
		oui::VectorFont italic;
		oui::VectorFont bold;
	public:
		std::vector<Float*> floats;

		float keysize = 6;

		short section = 0;
		short subsection = 0;

		float float_width;
		//Vector float_margin;
		//Vector float_pen;

		Context(oui::Window& window) : _w(&window),
			mono{ "fonts/LinLibertine_Mah.ttf" },
			sans{ "fonts/LinBiolinum_Rah.ttf" },
			roman{ "fonts/LinLibertine_Rah.ttf" },
			italic{ "fonts/LinLibertine_RIah.ttf" },
			bold{ "fonts/LinLibertine_RBah.ttf" } {}

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
			case FontType::bold: return &bold;
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
