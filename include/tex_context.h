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
		std::vector<Float*> floats;
		Owner<Line> lines;

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

}
