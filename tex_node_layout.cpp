#include <tex_node.h>
#include <tex_paragraph.h>
#include <find.h>

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

			auto jt = it;
			while (jt != rest.end() && space(*jt))
				++jt;
			position(jt == rest.end() ? Align::left : Align::justified);

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
	float Paragraph::updateLayout(oui::Vector pen, float indent, float width)
	{
		LineBulider builder(pen, begin(), end());

		while (!builder.done())
		{
			builder.buildLine(pen.x + indent, width - indent);
			indent = 0;
		}

		return builder.height();
	}


	bool Node::collect(Paragraph& out)
	{
		out.push_back(this);
		return true;
	}
	bool Group::collect(Paragraph& out)
	{
		for (auto&& e : *this)
			e.collect(out);
		return true;
	}
	bool Space::collect(Paragraph& out)
	{
		if (count(space, '\n') >= 2)
			return false;
		out.push_back(this);
		return true;
	}

	void Command::updateSize(Context& con, Mode, Font font, float /*width*/)
	{
		const auto F = con.font(font);
		box.width(F->offset(cmd, con.ptsize(font)), align::min);
		box.height(con.ptsize(font), align::center);
	}
	void Text::updateSize(Context& con, Mode new_mode, Font new_font, float /*width*/)
	{
		mode = new_mode;
		font = new_font;
		const auto F = con.font(font);
		box.width(F->offset(text, con.ptsize(font)), align::min);
		box.height(con.ptsize(font), align::center);
	}
	void Space::updateSize(Context& con, Mode mode, Font font, float /*width*/)
	{
		if (count(space, '\n') >= 2)
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
		box.before = box.after = 0;
		for (auto&& e : *this)
		{
			e.updateSize(con, mode, font, width);
			box.above = std::max(box.above, e.box.above);
			box.below = std::max(box.below, e.box.below);
			box.after += e.box.width();
		}
	}
	void Par::updateSize(Context & con, Mode mode, Font font, float width)
	{
		static const Font styles[] = 
		{
			{ FontType::roman, FontSize::normalsize }, // simple
			{ FontType::roman, FontSize::Huge }, // title
			{ FontType::bold, FontSize::LARGE }, // author
			{ FontType::bold, FontSize::Large }, // section
			{ FontType::bold, FontSize::large } // subsection
		};
		_font = font = gsl::at(styles, _code(_type));

		if (_type == Type::section)
		{
			con.section += 1;
			_pretitle = std::to_string(con.section) + ' ';
		}
		else if (_type == Type::subsection)
		{
			con.subsection += 1;
			_pretitle = std::to_string(con.section) + '.' + std::to_string(con.subsection) + ' ';
		}

		const auto em = con.ptsize(font);

		_parindent = (_type == Type::simple) ? 1.5f*em : con.font(font)->offset(_pretitle, em);

		box.above = 0;
		box.below = em;
		box.before = 0;
		box.after = width;

		for (auto&& sub : *this)
			sub.updateSize(con, mode, font, width);
	}


	void Node::updateLayout(oui::Vector offset)
	{
		box.offset = offset;
	}

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
	void Par::updateLayout(oui::Vector offset)
	{
		box.offset = offset;

		const float width = box.width();
		oui::Vector pen = { 0, 0 };

		Paragraph par;

		float indent = _parindent;

		for (auto it = begin(); it != end(); ++it)
		{
			par.clear();
			if (it->collect(par))
			{
				++it;
				while (it != end() && it->collect(par))
					++it;
				pen.y = par.updateLayout(pen, indent, width);
				indent = 0;
				if (it == end())  break;
			}
			it->updateLayout(pen);
			pen.y += it->box.height();
		}
		box.height(pen.y, align::min);
	}


	void Group::render(tex::Context& con, oui::Vector offset) const
	{
		offset = offset + box.offset;

		for (auto&& e : *this)
			e.render(con, offset);
	}
	void Par::render(Context & con, oui::Vector offset) const
	{
		if (!_pretitle.empty())
		{
			using namespace oui::colors;
			static constexpr auto color = mix(white, black, 0.6);
			con.font(_font)->drawLine(box.min() + offset, _pretitle, con.ptsize(_font), color);
		}
		Group::render(con, offset);
	}

	void Command::render(tex::Context& con, oui::Vector offset) const
	{
		con.font(tex::FontType::sans)->drawLine(offset + box.min(), cmd,
			oui::Color{ .3f, .9f, .1f }, con.ptsize(tex::FontSize::normalsize));
	}
	void Text::render(tex::Context& con, oui::Vector offset) const
	{
		con.font(font.type)->drawLine(offset + box.min(), text,
			oui::colors::black, con.ptsize(font.size));
	}
}