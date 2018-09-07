#include "tex_node_internal.h"
#include <tex_paragraph.h>

namespace tex
{
	namespace align = oui::align;

	class Curly : public Group
	{
	public:
		Curly(string) { }

		bool collect(Paragraph& out)
		{
			for (auto&& e : *this)
				out.push_back(&e);
			return true;
		}

		void enforceRules() final { _enforce_child_rules(); }
		bool terminatedBy(string_view token) const final { return token == "}"; }

		Box& updateSize(Context& con, Mode mode, Font font, float width) final
		{
			_box.before = _box.after = 0;
			_box.above = _box.below = 0;

			for (auto&& sub : *this)
			{
				auto& sbox = sub.updateSize(con, mode, font, width);

				if (_box.above < sbox.above)
					_box.above = sbox.above;
				if (_box.below < sbox.below)
					_box.below = sbox.below;

				sbox.offset = { _box.after, 0 };
				_box.after += sbox.width();
			}

			return _box;
		}

		void serialize(std::ostream& out) const final
		{
			out << '{';
			Group::serialize(out);
			out << '}';
		}
	};

	class Float : public Group
	{
		Box _float_box;
		Owner<Line> _lines;
	protected:
		Text* _exit_this_or_next_text() noexcept override { return nullptr; }
		Text* _exit_this_or_prev_text() noexcept override { return nullptr; }

		Text* _this_or_prev_text() noexcept override { return prevText(); };
		Text* _this_or_next_text() noexcept override { return nextText(); };
		Text* _exit_this_or_next_stop() noexcept override { return nextText(); }
		Text* _exit_this_or_prev_stop() noexcept override { return prevText(); }

		Text* _this_or_prev_stop() noexcept override 
		{ 
			Expects(!empty()); 
			auto text = as<Text>(&back());
			Ensures(text);
			return text;
		};
		Text* _this_or_next_stop() noexcept override 
		{ 
			Expects(!empty()); 
			auto text = as<Text>(&front());
			Ensures(text);
			return text;
		};
	public:


		void floatOffset(Vector offset) { _float_box.offset = offset; }

		const Box& contentBox() const override { return _float_box; }

		Box& updateSize(Context& con, Mode mode, Font font, float width) override
		{
			con.floats.push_back(this);

			font.size = shift(font.size, -2);

			const auto em = con.ptsize(font);
			_box.above = _box.below = em * 0.5f;
			_box.before = 0;
			_box.after = em * 0.125f;

			for (auto&& sub : *this)
				sub.updateSize(con, mode, font, width);

			auto lines_backup = std::exchange(con.lines, nullptr);

			_float_box.width(con.float_width, align::min);
			_float_box.height(layoutParagraph(con, this, 0, _float_box.width()), align::min);

			_lines = std::exchange(con.lines, move(lines_backup));

			return _box;
		}

		void render(tex::Context& con, oui::Vector offset) const final
		{
			constexpr auto color = oui::Color{ 1, 0.8, 0.1 };
			constexpr auto padding = Vector{ 2, 2 };
			const Rectangle lbox =
			{
				offset + _box.min(),
				offset + _box.max()
			};
			const Rectangle cbox =
			{
				offset + _float_box.min() - padding,
				offset + _float_box.max() + padding
			};

			const auto bend = Point{ cbox.min.x - 10, lbox.max.y };

			oui::fill(lbox, color);
			oui::fill(cbox, color);
			oui::line(lbox.max, bend, color, 2);
			oui::line(bend, Point{ cbox.min.x, std::clamp(bend.y, cbox.min.y, cbox.max.y) }, color, 2);

			Group::render(con, offset + (_float_box.offset - _box.offset));
		}
	};

	class Comment : public Float
	{
	public:
		string terminator;

		Comment(string) { }

		bool terminatedBy(string_view) const final { return false; }

		void enforceRules() final 
		{ 
			while (auto space = as<Space>(empty() ? nullptr : &back()))
			{
				terminator.insert(0, space->space);
				back().removeFromGroup();
			}
			Float::enforceRules();
		}

		void serialize(std::ostream& out) const final
		{
			out << '%';
			Group::serialize(out);
			out << terminator;
		}
	};

	class Math : public Group
	{
	public:
		Math(string) { }

		bool terminatedBy(string_view token) const final { return token == "$"; }

		Box& updateSize(Context& con, Mode mode, Font font, float width) final
		{
			mode = Mode::math;
			font.type = FontType::italic;

			Group::updateSize(con, mode, font, width);
			return _box;
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

		Box& updateSize(Context& con, Mode mode, Font font, float width) final
		{
			Expects(mode == Mode::math);
			const auto p = front().getArgument();
			const auto q = p->group.next()->getArgument();

			font.size = shift(font.size, -2);

			auto& pbox = p->updateSize(con, Mode::math, font, width);
			auto& qbox = q->updateSize(con, Mode::math, font, width);

			_box.width(std::max(pbox.width(), qbox.width()), align::min);
			_box.above = pbox.height() + con.ptsize(font)*0.05f;
			_box.below = qbox.height() + con.ptsize(font)*0.05f;

			pbox.offset = { (_box.width() - pbox.width())*0.5f, -pbox.below - (_box.above - pbox.height()) };
			qbox.offset = { (_box.width() - qbox.width())*0.5f, +qbox.above + (_box.below - pbox.height()) };

			return _box;
		}

		void render(tex::Context& con, oui::Vector offset) const final
		{
			Group::render(con, offset);
			oui::fill(oui::align::centerLeft(oui::origo + offset + _box.offset).size({ _box.width(), 1 }), 
				oui::colors::black);
		}
	};
	class VerticalGroup : public Group
	{
	protected:
		Box& _vertical_layout(Context& con, Mode mode, Font font)
		{
			float height = 0;
			for (auto&& sub : *this)
			{
				auto& sbox = sub.updateSize(con, mode, font, _box.width());
				_box.below += sbox.height();

				const auto sub_align = sbox.before / sbox.width();
				sbox.offset = { (sub_align - 0.5f)*_box.width(), height + sbox.above };
				height += sbox.height();
			}
			_box.above = 0;
			_box.below = height;

			return _box;
		}
	public:
		VerticalGroup(string) { }

		Flow flow() const noexcept final { return Flow::vertical; }

		Box& updateSize(Context& con, Mode mode, Font font, float width) override
		{
			_box.width(width, align::center);
			_box.above = _box.below = 0;

			return _vertical_layout(con, mode, font);
		}
	};

	class Root : public VerticalGroup
	{
	public:
		using VerticalGroup::VerticalGroup;

		bool terminatedBy(string_view) const final { return false; }

		Box& updateSize(Context& con, Mode mode, Font font, float width) final
		{
			con.floats.clear();
			con.lines = nullptr;
			con.section = 0;
			con.subsection = 0;


			const auto em = con.ptsize(font);
			con.float_width = std::min(width*0.3f, em * 12);
			const auto main_width = width - (con.float_width + em);


			_box.before = 0;
			_box.after = width;
			_box.above = _box.below = 0;


			float height = 0;
			float line_height = 0;
			float line_offset = em;
			for (auto&& sub : *this)
			{
				auto& sbox = sub.updateSize(con, mode, font, main_width-em);
				if (sub.flow() == Flow::vertical)
				{
					height += line_height;
					line_height = 0;
					line_offset = em;
					const auto sub_align = sbox.before / sbox.width();
					sbox.offset = { (main_width-em)*sub_align + 0.5f*em, height + sbox.above };
					height += sbox.height();
				}
				else
				{
					sbox.offset = { line_offset + sbox.before, height + sbox.above };
					line_offset += sbox.width();
					line_height = std::max(line_height, sbox.height());
				}
			}
			height += line_height;
			_box.above = 0;
			_box.below = height;

			// pass 2: place floats
			Vector pen = { width - (con.float_width + 0.5f*em) , 0 };
			for (auto&& sub : con.floats)
			{
				auto& lbox = sub->layoutBox();
				auto loff = lbox.offset;
				for (auto p = sub->group(); p && p != this; p = p->group())
					loff += p->layoutBox().offset;

				loff.y -= lbox.above;

				if (pen.y < loff.y)
					pen.y = loff.y;

				loff.y += lbox.above;
				loff += -lbox.offset;

				sub->floatOffset(pen - loff);

				pen.y += sub->contentBox().height() + em;
			}
			con.floats.clear();

			return _box;
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

		Box& updateSize(Context& con, Mode mode, Font font, float width) override
		{
			font.type = FontType::roman;
			_box.width(std::min(width, con.ptsize(font) * 24.0f), align::center);
			_box.above = _box.below = 0;

			return _vertical_layout(con, mode, font);
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
		return intrusive::refcount::make<G>(std::move(name));
	}
	Owner<Group> Group::make(string name)
	{
		static constexpr frozen::unordered_map<string_view, Owner<Group>(*)(string), 10>
			maker_lookup =
		{
		{ "%", make_group<Comment> },
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
