#pragma once

#include <string>
#include <stdexcept>
#include <string_view>

#include <oui_window.h>

#include "small_string.h"

#include "tex_util.h"

namespace tex
{
	using oui::Vector;
	using oui::Point;
	using oui::Rectangle;

	using std::string_view;

	using string = SmallString;

	template <class C>
	constexpr int int_size(C&& c) { return narrow<int>(std::size(c)); }

	class IllFormed : public std::exception
	{
		std::string _message;
	public:

		template <class... Args>
		IllFormed(Args&&... args) 		{
			_message.reserve((std::size(args) + ...));
			(_message.append(args), ...);
		}

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

		constexpr Font() : type(FontType::roman), size(FontSize::normalsize) { }
		constexpr Font(FontType t, FontSize s = FontSize::normalsize) : type(t), size(s) { }
		constexpr Font(FontSize s, FontType t = FontType::roman)      : type(t), size(s) { }
	};

	inline float ptsize(FontSize size, float key) noexcept
	{ 
		return key * pow(2.0f, static_cast<char>(size) / 5.0f);
	}

	class Box
	{
	public:
		oui::Vector offset;
		float before = 0;
		float above  = 0;
		float after  = 0;
		float below  = 0;

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
