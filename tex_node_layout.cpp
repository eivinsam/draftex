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
		Vector pen;
		xpr::Range<iterator, iterator> rest;
		iterator it;

		enum class Align { left, justified };
	public:
		LineBulider(Vector pen, iterator par_begin, iterator par_end) :
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
				auto& lbox = it->layoutBox();
				const auto box_width = lbox.width();
				if (width_left < box_width && it != rest.begin())
					break;
				space_count += space(*it) ? 1 : 0;
				max_above = std::max(max_above, lbox.above);
				max_below = std::max(max_below, lbox.below);
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
				width_left += it->layoutBox().width();
				space_count -= 1;
			}
			++it;
		}
		void position(const Align alignment)
		{
			pen.y += max_above;
			for (auto&& e : currentLine())
			{
				e.layoutOffset(pen);
				pen.x += e.layoutBox().width();

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
	float Paragraph::updateLayout(Vector pen, float indent, float width)
	{
		LineBulider builder(pen, begin(), end());

		if (builder.done())
			return 0;

		builder.buildLine(pen.x + indent, width - indent);
		while (!builder.done())
			builder.buildLine(pen.x, width);

		return builder.height();
	}

	float layoutParagraph(Group* p, float indent, float width)
	{
		Paragraph par;

		Vector pen = { 0, 0 };

		for (auto it = p->begin(); it != p->end(); ++it)
		{
			par.clear();
			if (it->collect(par))
			{
				++it;
				while (it != p->end() && it->collect(par))
					++it;
				pen.y = par.updateLayout(pen, indent, width);
				indent = 0;
				if (it == p->end())  break;
			}
			it->layoutOffset(pen);
			pen.y += it->layoutBox().height();
		}
		return pen.y;
	}

	bool Node::collect(Paragraph& out)
	{
		out.push_back(this);
		return true;
	}
	bool Group::collect(Paragraph& out)
	{
		out.push_back(this);
		return true;
	}
	bool Space::collect(Paragraph& out)
	{
		if (count(space, '\n') >= 2)
			return false;
		out.push_back(this);
		return true;
	}

	Box& Command::updateSize(Context& con, Mode, Font font, float /*width*/)
	{
		const auto F = con.font(font);
		_box.width(F->offset(cmd, con.ptsize(font)), align::min);
		_box.height(con.ptsize(font), align::center);
		return _box;
	}
	Box& Text::updateSize(Context& con, Mode new_mode, Font new_font, float /*width*/)
	{
		mode = new_mode;
		font = new_font;
		const auto F = con.font(font);
		_box.width(F->offset(text, con.ptsize(font)), align::min);
		_box.height(con.ptsize(font), align::center);
		return _box;
	}
	Box& Space::updateSize(Context& con, Mode mode, Font font, float /*width*/)
	{
		if (count(space, '\n') >= 2)
		{
			Expects(mode != Mode::math);
			_box.width(0, align::min);
			_box.height(0, align::min);
			return _box;
		}
		_box.width(con.ptsize(font)*(mode == Mode::math ? 0 : 0.25f), align::min);
		_box.height(con.ptsize(font), align::center);
		return _box;
	}


	Box& Group::updateSize(Context& con, Mode mode, Font font, float width)
	{
		_box.before = _box.after = 0;
		for (auto&& e : *this)
		{
			auto& ebox = e.updateSize(con, mode, font, width);;
			_box.above = std::max(_box.above, ebox.above);
			_box.below = std::max(_box.below, ebox.below);
			_box.after += ebox.width();
		}
		return _box;
	}
	void Par::partype(Type t) 
	{ 
		if (_type == t)
			return;
		Expects(!empty());

		auto terminator = as<Space>(&back());
		if (!terminator)
			terminator = append(Space::make());

		if (_type == Type::simple)
		{
			terminator->space = "\n";

			const auto new_curl = front().insertBefore(Group::make("curly"));
			while (new_curl->next() != terminator)
				new_curl->append(new_curl->next()->detach());
		}
		else if (t == Type::simple)
		{
			terminator->space = "\n\n";

			const auto curly = as<Group>(&front());
			while (!curly->empty())
				curly->insertAfter(curly->back().detach());
			curly->remove();
		}

		_type = t; 

		change(); 
	}
	Box& Par::updateSize(Context & con, Mode mode, Font font, float width)
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
		else
			_pretitle = "";

		const auto em = con.ptsize(font);

		_parindent = 0;
		if (_type == Type::simple)
		{
			if (auto pp = as<Par>(prev()); pp && pp->_type == Type::simple)
				_parindent = 1.5f*em;
		}
		else 
			_parindent = con.font(font)->offset(_pretitle, em);

		_box.above = 0;
		_box.below = em;
		_box.before = 0;
		_box.after = width;

		for (auto&& sub : *this)
			sub.updateSize(con, mode, font, width);

		_box.height(layoutParagraph(this, _parindent, _box.width()), align::min);

		return _box;
	}


	void Group::render(tex::Context& con, Vector offset) const
	{
		offset = offset + _box.offset;

		for (auto&& e : *this)
			e.render(con, offset);
	}
	void Par::render(Context & con, Vector offset) const
	{
		if (!_pretitle.empty())
		{
			using namespace oui::colors;
			static constexpr auto color = mix(white, black, 0.6);
			con.font(_font)->drawLine(_box.min() + offset, _pretitle, con.ptsize(_font), color);
		}

		Group::render(con, offset);

	}

	void Command::render(Context& con, Vector offset) const
	{
		con.font(tex::FontType::sans)->drawLine(offset + _box.min(), cmd,
			oui::Color{ .3f, .9f, .1f }, con.ptsize(tex::FontSize::normalsize));
	}
	void Text::render(tex::Context& con, oui::Vector offset) const
	{
		con.font(font.type)->drawLine(offset + _box.min(), text,
			oui::colors::black, con.ptsize(font.size));
	}
}