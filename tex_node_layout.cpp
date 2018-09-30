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
	Context::Context(oui::Window& window) : _w(&window),
		mono{ "fonts/LinLibertine_Mah.ttf" },
		sans{ "fonts/LinBiolinum_Rah.ttf" },
		roman{ "fonts/LinLibertine_Rah.ttf" },
		italic{ "fonts/LinLibertine_RIah.ttf" },
		bold{ "fonts/LinLibertine_RBah.ttf" },
		mode{ Mode::text },
		font_type{ FontType::sans },
		font_size{ FontSize::normalsize }
	{}

	namespace align = oui::align;

	class LineBulider
	{
		using iterator = Paragraph::iterator;
		Context& _con;
		int space_count = 0;
		float max_above = 0;
		float max_below = 0;
		float width_left = 0;
		Vector pen;
		xpr::Range<iterator, iterator> rest;
		iterator it;

		enum class Align { left, justified };
	public:
		LineBulider(Context& con, Vector pen, iterator par_begin, iterator par_end) :
			_con(con), pen(pen), rest(par_begin, par_end) { }

		constexpr bool    done() const noexcept { return rest.empty(); }
		constexpr float height() const noexcept { return pen.y; }

		auto currentLine()
		{
			return xpr::those.from(rest.begin()).until(it);
		}

		void buildLine(float start_x, float width)
		{
			reset(start_x, width);

			if (rest.empty())
				return;

			collectLine();

			position(it == rest.end() ? Align::left : Align::justified);

			rest.first = it;
		}
	private:
		void reset(float start_x, float width) noexcept
		{
			space_count = 0;
			max_above = 0;
			max_below = 0;
			pen.x = start_x;
			width_left = width;
			it = rest.first;
		}
		void collectLine()
		{
			push(_con.lines, intrusive::refcount::make<Line>());
			const Text* last = nullptr;
			for (it = rest.begin(); it != rest.end(); ++it)
			{
				auto& lbox = it->layoutBox();
				const auto box_width = lbox.width();
				if (width_left < box_width && it != rest.begin())
					break;
				if (auto text = as<Text>(&*it))
				{
					last = text;
					if (!text->space().empty())
						++space_count;
				}
				max_above = std::max(max_above, lbox.above);
				max_below = std::max(max_below, lbox.below);
				width_left -= box_width;

				if (auto text = as<Text>(&*it))
					_con.lines->append(intrusive::refcount::claim(text));
			}
			if (last && !last->space().empty())
				space_count -= 1;
		}
		void position(const Align alignment)
		{
			pen.y += max_above;
			for (auto&& e : currentLine())
			{
				e.layoutOffset(pen);
				if (space_count > 0 && alignment == Align::justified)
				{
					if (auto text = as<Text>(&e); text && !text->space().empty())
					{
						const float incr = width_left / space_count;
						width_left -= incr;
						space_count -= 1;
						e.widen(incr);
					}
				}
				pen.x += e.layoutBox().width();
			}
			pen.y += max_below;
		}
	};
	float Paragraph::updateLayout(Context& con, Vector pen, float indent, float width)
	{
		LineBulider builder(con, pen, begin(), end());

		if (builder.done())
			return 0;

		builder.buildLine(pen.x + indent, width - indent);
		while (!builder.done())
			builder.buildLine(pen.x, width);

		return builder.height();
	}

	float layoutParagraph(Context& con, nonull<const Group*> p, float indent, float width)
	{
		Paragraph par;

		Vector pen = { 0, 0 };

		for (auto it = p->begin(); it != p->end(); )
		{
			par.clear();
			if (!it->collect(par))
			{
				it->layoutOffset(pen);
				pen.y += it->layoutBox().height();
				++it;
				continue;
			}

			for (++it; ; ++it) 
			{
				if (it == p->end() || !it->collect(par))
					break;
			}

			pen.y = par.updateLayout(con, pen, indent, width);
			indent = 0;
		}
		return pen.y;
	}

	bool Node::collect(Paragraph& out) const
	{
		out.push_back(this);
		return true;
	}
	bool Group::collect(Paragraph& out) const
	{
		out.push_back(this);
		return true;
	}

	std::optional<string> Command::asEnd() const noexcept
	{
		if (cmd.text().size() > 4 && string_view(cmd.text()).substr(0, 4) == "end ")
			return cmd.text().substr(4);
		else
			return {};
	}

	Box& Command::updateLayout(Context& con) const
	{
		_font_size = con.font_size;
		const auto em = con.ptsize(_font_size);
		const auto F = con.fontData(FontType::sans);
		_box.width(F->offset(cmd.text(), em), align::min);
		_box.height(em, align::center);
		return _box;
	}
	Box& Text::updateLayout(Context& con) const 
	{
		mode = con.mode;
		font = con.font();
		const auto F = con.fontData(font);
		_box.width(F->offset(text(), con.ptsize(font)), align::min);
		if (!space().empty())
			_box.after += F->offset(" ", con.ptsize(font));
		_box.height(con.ptsize(font), align::center);
		return _box;
	}


	Box& Group::updateLayout(Context& con) const
	{
		_box.before = _box.after = 0;
		for (auto&& e : *this)
		{
			const auto& ebox = e.updateLayout(con);
			e._box.offset = { _box.after, 0 };
			_box.above = std::max(_box.above, ebox.above);
			_box.below = std::max(_box.below, ebox.below);
			_box.after += ebox.width();
		}
		return _box;
	}
	void Par::partype(Type t) noexcept
	{ 
		if (_type == t)
			return;

		_type = t; 

		markChange(); 
	}
	Box& Par::updateLayout(Context & con) const
	{
		static const Font styles[] = 
		{
			{ FontType::roman, FontSize::normalsize }, // simple
			{ FontType::roman, FontSize::Huge }, // title
			{ FontType::bold, FontSize::LARGE }, // author
			{ FontType::bold, FontSize::Large }, // section
			{ FontType::bold, FontSize::large } // subsection
		};
		_font = gsl::at(styles, _code(_type));
		auto old_type = con.font_type.push(_font.type);
		auto old_size = con.font_size.push(_font.size);

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

		const auto em = con.ptsize(_font);

		_parindent = 0;
		if (_type == Type::simple)
		{
			if (auto pp = as<Par>(group.prev()); pp && pp->_type == Type::simple)
				_parindent = 1.5f*em;
		}
		else 
			_parindent = con.fontData(_font)->offset(_pretitle, em);

		_box.above = 0;
		_box.below = em;
		_box.before = 0;
		_box.after = con.width;

		for (auto&& sub : *this)
			sub.updateLayout(con);

		_box.height(layoutParagraph(con, this, _parindent, _box.width()), align::min);

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
			con.fontData(_font)->drawLine(_box.min() + offset, _pretitle, con.ptsize(_font), color);
		}

		Group::render(con, offset);

	}
	void Par::serialize(std::ostream & out) const
	{
		out << name(_type);
		if (_type != Type::simple)
			out << '{';
		Group::serialize(out);
		if (_type != Type::simple)
			out << '}';
		out << terminator;
	}

	void Command::render(Context& con, Vector offset) const
	{
		con.fontData(tex::FontType::sans)->drawLine(offset + _box.min(), cmd.text(),
			oui::Color{ .3f, .9f, .1f }, con.ptsize(_font_size));
	}
	void Text::render(tex::Context& con, oui::Vector offset) const
	{
		con.fontData(font.type)->drawLine(offset + _box.min(), text(),
			oui::colors::black, con.ptsize(font.size));
	}
}