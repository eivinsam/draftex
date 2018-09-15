#pragma once

#include <oui_text.h>

#include "tex.h"

namespace tex
{
	class Float;
	class Line;

	class Context
	{
		oui::Window* _w;
		oui::VectorFont mono;
		oui::VectorFont sans;
		oui::VectorFont roman;
		oui::VectorFont italic;
		oui::VectorFont bold;
	public:
		template <class T>
		class Param
		{
			T _value;
		public:
			constexpr Param()
			{
				if constexpr (std::is_arithmetic_v<T>)
					_value = 0;
			}
			constexpr Param(T initial) : _value(std::move(initial)) { }

			Param(Param&&) = delete;
			Param(const Param&) = delete;
			Param& operator=(Param&&) = delete;
			Param& operator=(const Param&) = delete;

			class Old
			{
				friend class Param;
				T* _loc;
				T _value;
				Old(T* loc) : _loc(loc), _value(std::move(*loc)) { }
			public:
				Old(const Old&&) = delete;
				Old(Old&& other) : _loc(other._loc), _value(std::move(other._value)) { other._loc = nullptr; }
				Old& operator=(Old&&) = delete;
				Old& operator=(const Old&) = delete;

				~Old() { if (_loc) *_loc = std::move(_value); }
			};

			const T* operator->() const { return &_value; }

			operator const T&() const { return _value; }

			template <class S> friend bool operator==(const Param& a, const S& b) { return a._value == b; }
			template <class S> friend bool operator!=(const Param& a, const S& b) { return a._value != b; }

			template <class S> friend bool operator==(const S& a, const Param& b) { return a == b._value; }
			template <class S> friend bool operator!=(const S& a, const Param& b) { return a != b._value; }

			Old push(T new_value) { Old backup(&_value); _value = std::move(new_value); return backup; }
		};



		std::vector<Float*> floats;

		Param<Mode> mode;
		Param<FontType> font_type;
		Param<FontSize> font_size;
		Param<float> width;

		Owner<Line> lines;


		float keysize = 6;

		short section = 0;
		short subsection = 0;
		short footnote = 0;

		float float_width;
		//Vector float_margin;
		//Vector float_pen;

		Context(oui::Window& window);

		void reset(oui::Window& window) noexcept
		{
			_w = &window;
		}

		Font font() const { return { font_type, font_size }; }

		gsl::not_null<oui::VectorFont*> 
			fontData(FontType f)
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
		auto fontData(Font f) { return fontData(f.type); }
		auto fontData() { return fontData(font_type); }

		oui::Window& window() const noexcept { return *_w; }

		float ptsize(FontSize size) const { return tex::ptsize(size, keysize); }
		float ptsize(Font f) const { return ptsize(f.size); }
		float ptsize() const { return ptsize(font_size); }
	};

}
