#include <tex_node.h>
#include <tex_paragraph.h>
#include <find.h>

namespace tex
{
	namespace align = oui::align;

	using std::make_unique;
	using std::string;
	using std::string_view;

	class Frac : public Group
	{
	public:
		void updateSize(Context& con, Mode mode, Font font, float width) final
		{
			Expects(mode == Mode::math);
			const auto p = front().getArgument();
			const auto q = p->next->getArgument();

			font.size = shift(font.size, -2);

			p->updateSize(con, Mode::math, font, width);
			q->updateSize(con, Mode::math, font, width);

			box.width(std::max(p->box.width(), q->box.width()), align::min);
			box.above = p->box.height() + con.ptsize(font)*0.05f;
			box.below = q->box.height() + con.ptsize(font)*0.05f;
		}

		void updateLayout(oui::Vector offset) final
		{
			box.offset = offset;

			Node*const p = front().getArgument();
			Node*const q = p->next->getArgument();

			p->updateLayout({ (box.width() - p->box.width())*0.5f, -p->box.below - (box.above-p->box.height()) });
			q->updateLayout({ (box.width() - q->box.width())*0.5f, +q->box.above + (box.below-p->box.height()) });
		}

		void serialize(std::ostream& out) const final
		{
			out << "\\frac";
			for (auto&& e : *this)
				e.serialize(out);
		}
		void render(tex::Context& con, oui::Vector offset) const final
		{
			Group::render(con, offset);
			oui::fill(oui::align::centerLeft(oui::origo + offset + box.offset).size({ box.width(), 1 }), oui::colors::black);
		}
	};
	class VerticalGroup : public Group
	{
	public:
		void updateSize(Context& con, Mode mode, Font font, float width) final
		{
			if (data == "root")
			{
				con.section = 0;
				con.subsection = 0;
			}

			if (data == "document")
			{
				font.type = FontType::roman;
				box.width(std::min(width, con.ptsize(font) * 24.0f), align::center);
				box.above = box.below = 0;
			}
			else
			{
				box.width(width, align::center);
				box.above = box.below = 0;
			}

			for (auto&& sub : *this)
			{
				sub.updateSize(con, mode, font, box.width());
				box.below += sub.box.height();
			}
		}
		void updateLayout(oui::Vector offset) final
		{
			box.offset = offset;

			float height = 0;
			for (auto&& sub : *this)
			{
				const auto sub_align = sub.box.before / sub.box.width();
				sub.updateLayout({ (sub_align - 0.5f)*box.width(), height + sub.box.above });
				height += sub.box.height();
			}
			box.above = 0;
			box.below = height;
		}
		void serialize(std::ostream & out) const final
		{
			if (data == "document")
			{
				out << "\\begin{" << data << "}";
				_serialize_children(out);
				out << "\\end{" << data << "}";
				return;
			}
			_serialize_children(out);
		}
	};
	class Par : public Group
	{
	protected:
		bool _needs_text_before(Node*) const final { return false; }
		float _parindent = 0;
	public:
		void updateSize(Context& con, Mode mode, Font font, float width) override
		{
			_parindent = 1.5f * con.ptsize(font);
			box.width(width, align::min);
			box.height(0, align::min);

			for (auto&& sub : *this)
				sub.updateSize(con, mode, font, width);
		}
		void updateLayout(oui::Vector offset) final
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
	};

	class Heading : public Par
	{
		Font _font;
		std::string _pretitle;
	public:
		bool collect(Paragraph&) final { return false; }

		void updateSize(Context& con, Mode mode, Font font, float width) final
		{
			static const std::unordered_map<string_view, Font> styles = 
			{
			{ "title",      { FontType::bold, FontSize::Huge } },
			{ "author",     { FontType::bold, FontSize::LARGE } },
			{ "section",    { FontType::bold, FontSize::Large } },
			{ "subsection", { FontType::bold, FontSize::large } }
			};

			if (data == "section")
			{
				con.section += 1;
				_pretitle = std::to_string(con.section) + ' ';
			}
			else if (data == "subsection")
			{
				con.subsection += 1;
				_pretitle = std::to_string(con.section) + '.' + std::to_string(con.subsection) + ' ';
			}

			_font = font = find(styles, data, default_value=font);

			const auto face = con.font(font);
			const auto em = con.ptsize(font);

			_parindent = face->offset(_pretitle, em);

			box.above = 0;
			box.below = em;
			box.before = 0;
			box.after = width;
			for (auto&& sub : *this)
				sub.updateSize(con, mode, font, width);
		}

		void render(Context& con, oui::Vector offset) const final
		{
			static constexpr auto color = mix(oui::colors::white, oui::colors::black, 0.6);
			con.font(_font)->drawLine(box.min() + offset, _pretitle, con.ptsize(_font), color);
			Par::render(con, offset);
		}

		void serialize(std::ostream& out) const final
		{
			out << '\\' << data;
			_serialize_children(out);
		}
	};


	template <class G>
	Owner<Group> make_group(string name)
	{
		auto result = make_unique<G>();
		result->data = move(name);
		return result;
	}
	Owner<Group> Group::make(string name)
	{
		static const std::unordered_map<string_view, Owner<Group>(*)(string)>
			maker_lookup =
		{
		{ "frac", make_group<Frac> },
		{ "par", make_group<Par> },
		{ "root", make_group<VerticalGroup> },
		{ "document", make_group<VerticalGroup> },
		{ "title", make_group<Heading> },
		{ "author", make_group<Heading> },
		{ "section", make_group<Heading> },
		{ "subsection", make_group<Heading> }
		};

		return find(maker_lookup, name, default_value = &make_group<Group>)(move(name));
	}

}
