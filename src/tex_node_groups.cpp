#include "tex_node_internal.h"
#include <tex_paragraph.h>

namespace tex
{
	namespace align = oui::align;

	class Curly : public Group
	{
	public:
		Curly(string) { }

		void enforceRules() final { _enforce_child_rules(); }
		bool terminatedBy(string_view token) const final { return token == "}"; }

		void serialize(std::ostream& out) const final
		{
			out << '{';
			Group::serialize(out);
			out << '}';
		}
	};

	class Math : public Group
	{
	public:
		Math(string) { }

		bool terminatedBy(string_view token) const final { return token == "$"; }

		bool collect(Paragraph& out) final
		{
			out.push_back(this);
			return true;
		}

		void updateSize(Context& con, Mode mode, Font font, float width) final
		{
			mode = Mode::math;
			font.type = FontType::italic;

			Group::updateSize(con, mode, font, width);
		}

		void render(tex::Context& con, oui::Vector offset) const
		{
			oui::fill(absBox(), oui::Color{ 0.1f, 0.2f, 1.0f, 0.1f });
			
			Group::render(con, offset);
		}

		void serialize(std::ostream& out) const final
		{
			out << '$';
			Group::serialize(out);
			out << '$';
		}
	};

	class CommandGroup : public Group
	{
		string _cmd;
	public:
		CommandGroup(string cmd) : _cmd(std::move(cmd)) { }

		bool terminatedBy(string_view) const final { return false; }

		void serialize(std::ostream& out) const final
		{
			out << '\\' << _cmd;
			for (auto&& e : *this)
				e.serialize(out);
		}
	};

	class Frac : public CommandGroup
	{
	public:
		using CommandGroup::CommandGroup;

		bool collect(Paragraph& out) final
		{
			out.push_back(this);
			return true;
		}

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

		void render(tex::Context& con, oui::Vector offset) const final
		{
			Group::render(con, offset);
			oui::fill(oui::align::centerLeft(oui::origo + offset + box.offset).size({ box.width(), 1 }), oui::colors::black);
		}
	};
	class VerticalGroup : public Group
	{
	public:
		VerticalGroup(string) { }

		void updateSize(Context& con, Mode mode, Font font, float width) override
		{
			box.width(width, align::center);
			box.above = box.below = 0;

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
	};

	class Root : public VerticalGroup
	{
	public:
		using VerticalGroup::VerticalGroup;

		bool terminatedBy(string_view) const final { return false; }

		void updateSize(Context& con, Mode mode, Font font, float width) final
		{
			con.section = 0;
			con.subsection = 0;

			VerticalGroup::updateSize(con, mode, font, width);
		}
	};

	class Document : public VerticalGroup
	{
	public:
		using VerticalGroup::VerticalGroup;

		void enforceRules() final { _enforce_child_rules(); }

		void tokenize(string_view & in, Mode mode) final
		{
			static constexpr frozen::unordered_set<std::string_view, 4> headings =
			{
				"title",
				"author",
				"section",
				"subsection"
			};

			Owner<Group> par;
			while (!in.empty())
				if (auto sub = tokenize_single(in, *this, mode, OnEnd::match))
				{
					if (auto sp = as<Space>(sub.get()); sp && count(sp->space, '\n') >= 2)
					{
						if (par) append(move(par));
						par = Group::make("par");
					}
					else if (auto cmd = as<Command>(sub.get()); cmd && headings.count(cmd->cmd))
					{
						if (par) append(move(par));
						if (in.front() != '{')
							throw IllFormed(std::string("expected { after \\" + cmd->cmd));
						in.remove_prefix(1);
						auto arg = Group::make("curly");
						arg->tokenize(in, mode);
						auto headg = Group::make(cmd->cmd);
						headg->append(move(arg));
						if (isspace(in.front()))
							headg->append(tokenize_single(in, *this, mode, OnEnd::match));
						append(move(headg));

						continue;
					}

					if (!par) par = Group::make("par");
					par->append(move(sub));
				}
				else break;
			append(move(par));
		}
		bool terminatedBy(string_view token) const final { return token == "document"; }

		bool collect(Paragraph&) final { return false; }

		void updateSize(Context& con, Mode mode, Font font, float width) override
		{
			font.type = FontType::roman;
			box.width(std::min(width, con.ptsize(font) * 24.0f), align::center);
			box.above = box.below = 0;

			for (auto&& sub : *this)
			{
				sub.updateSize(con, mode, font, box.width());
				box.below += sub.box.height();
			}
		}

		void serialize(std::ostream& out) const final
		{
			out << "\\begin{document}";
			Group::serialize(out);
			out << "\\end{document}";
		}
	};


	template <class G>
	Owner<Group> make_group(string name)
	{
		return make_unique<G>(std::move(name));
	}
	Owner<Group> Group::make(string name)
	{
		static const std::unordered_map<string_view, Owner<Group>(*)(string)>
			maker_lookup =
		{
		{ "math", make_group<Math> },
		{ "frac", make_group<Frac> },
		{ "par", make_group<Par> },
		{ "root", make_group<Root> },
		{ "document", make_group<Document> },
		{ "title", make_group<Par> },
		{ "author", make_group<Par> },
		{ "section", make_group<Par> },
		{ "subsection", make_group<Par> }
		};

		return find(maker_lookup, name, default_value = &make_group<Curly>)(std::move(name));
	}
}
