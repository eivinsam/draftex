#include <tex_node.h>
#include <tex_paragraph.h>

using std::move;
using std::string;
using std::string_view;
using std::make_unique;


inline string_view view(const string& s) noexcept { return string_view{ s }; }

namespace tex
{
	namespace align = oui::align;

	class LineBulider
	{
		using iterator = Paragraph::iterator;
		int space_count;
		float max_above;
		float max_below;
		float width_left;
		oui::Vector pen;
		xpr::Range<iterator, iterator> rest;
		iterator it;

		enum class Align { left, justified };
	public:
		LineBulider(oui::Vector pen, iterator par_begin, iterator par_end) :
			pen(pen), rest(par_begin, par_end) { }

		bool done() const { return rest.empty(); }
		float height() const { return pen.y; }

		auto currentLine()
		{
			return xpr::those.from(rest.begin()).until(it);
		}

		void buildLine(float start_x, float width)
		{
			reset(start_x, width);

			skipSpaces();
			if (rest.empty())
				return;

			collectLine();

			position(it == rest.end() ? Align::left : Align::justified);

			rest.first = it;
		}
	private:
		void reset(float start_x, float width)
		{
			space_count = 0;
			max_above = 0;
			max_below = 0;
			pen.x = start_x;
			width_left = width;
			it = rest.first;
		}
		void skipSpaces()
		{
			while (it != rest.end() && space(*it))
				++it;
			rest.first = it;
		}
		void collectLine()
		{
			for (it = rest.begin(); it != rest.end(); ++it)
			{
				const auto box_width = it->box.width();
				if (width_left < box_width && it != rest.begin())
					break;
				space_count += space(*it) ? 1 : 0;
				max_above = std::max(max_above, it->box.above);
				max_below = std::max(max_below, it->box.below);
				width_left -= box_width;
			}
			unwindEndSpace();
		}
		void unwindEndSpace()
		{
			Expects(it != rest.begin());
			if (it != rest.end() && space(*it))
				return;

			--it;
			for (; space(*it); --it)
			{
				Expects(it != rest.begin());
				width_left += it->box.width();
				space_count -= 1;
			}
			++it;
		}
		void position(const Align alignment)
		{
			pen.y += max_above;
			for (auto&& e : currentLine())
			{
				e.updateLayout(pen);
				pen.x += e.box.width();

				if (alignment == Align::justified && space(e))
				{
					const float incr = width_left / space_count;
					width_left -= incr;
					space_count -= 1;
					pen.x += incr;
				}
			}
			pen.y += max_below;
		}
	};
	float Paragraph::updateLayout(oui::Vector pen, float width)
	{
		LineBulider builder(pen, begin(), end());

		while (!builder.done())
			builder.buildLine(pen.x, width);

		return builder.height();
	}


	bool Node::collect(Paragraph& out)
	{
		out.push_back(this);
		return true;
	}
	bool Group::collect(Paragraph& out)
	{
		if (data == "document" || data == "par")
			return false;
		if (data == "frac" || data == "math")
		{
			out.push_back(this);
			return true;
		}
		for (auto&& e : *this)
			e.collect(out);
		return true;
	}
	bool Space::collect(Paragraph& out)
	{
		if (count(data, '\n') >= 2)
			return false;
		out.push_back(this);
		return true;
	}

	void Node::updateSize(Context& con, Mode, Font font, float /*width*/)
	{
		const auto F = con.font(font);
		box.width(F->offset(data, con.ptsize(font)), align::min);
		box.height(con.ptsize(font), align::center);
	}
	void Text::updateSize(Context& con, Mode new_mode, Font new_font, float /*width*/)
	{
		mode = new_mode;
		font = new_font;
		const auto F = con.font(font);
		box.width(F->offset(data, con.ptsize(font)), align::min);
		box.height(con.ptsize(font), align::center);
	}
	void Space::updateSize(Context& con, Mode mode, Font font, float /*width*/)
	{
		if (count(data, '\n') >= 2)
		{
			Expects(mode != Mode::math);
			box.width(0, align::min);
			box.height(0, align::min);
			return;
		}
		box.width(con.ptsize(font)*(mode == Mode::math ? 0 : 0.25f), align::min);
		box.height(con.ptsize(font), align::center);
	}


	void Group::updateSize(Context& con, Mode mode, Font font, float width)
	{
		if (data == "math")
		{
			mode = Mode::math;
			font.type = FontType::italic;
		}
		box.before = box.after = 0;
		for (auto&& e : *this)
		{
			e.updateSize(con, mode, font, width);
			box.above = std::max(box.above, e.box.above);
			box.below = std::max(box.below, e.box.below);
			box.after += e.box.width();
		}
	}

	void Node::updateLayout(oui::Vector offset)
	{
		box.offset = offset;
	}

	using LayoutUpdater = void(*)(Group&, oui::Vector);

	void Group::updateLayout(oui::Vector offset)
	{
		box.offset = offset;
		oui::Vector pen = { 0, 0 };
		for (auto&& e : *this)
		{
			e.updateLayout(pen);
			pen.x += e.box.width();
		}
	}

}