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
		{ "title", make_group<Par> },
		{ "author", make_group<Par> },
		{ "section", make_group<Par> },
		{ "subsection", make_group<Par> }
		};

		return find(maker_lookup, name, default_value = &make_group<Group>)(move(name));
	}

}
