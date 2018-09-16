#include "tex_node_internal.h"
#include "tex_bib.h"
#include <tex_paragraph.h>
#include "file_mapping.h"

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

		Box& updateLayout(Context& con) final
		{
			_box.before = _box.after = 0;
			_box.above = _box.below = 0;

			for (auto&& sub : *this)
			{
				auto& sbox = sub.updateLayout(con);

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

	class Bibliography : public Group
	{
		std::optional<Bib> _data;
	public:
		Bibliography(string) { }

		bool terminatedBy(std::string_view) const final { return false; }

		const Bib::Entry* entry(string_view key)
		{
			if (!_data)
			{
				const auto text = as<Text>(&*begin());
				Expects(text && !text->group.next());

				_data.emplace(FileMapping((text->text + ".bib").c_str()).data);
			}
			return (*_data)[key];
		}
	};

	class Float : public Group
	{
		Box _float_box;
		Owner<Line> _lines;
		oui::Color _color;
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
		Float(const oui::Color& color) : _color(color) { }
	public:


		void floatOffset(Vector offset) { _float_box.offset = offset; }

		const Box& contentBox() const override { return _float_box; }

		Box& updateLayout(Context& con) override
		{
			con.floats.push_back(this);


			const auto em = con.ptsize();
			_box.above = _box.below = em * 0.5f;
			_box.before = 0;
			_box.after = em * 0.125f;

			auto old_size = con.font_size.push(shift(con.font_size, -2));
			for (auto&& sub : *this)
				sub.updateLayout(con);

			_lines = std::exchange(con.lines, nullptr);

			_float_box.width(con.float_width, align::min);
			_float_box.height(layoutParagraph(con, this, 0, _float_box.width()), align::min);

			_lines = std::exchange(con.lines, move(_lines));

			return _box;
		}

		void render(tex::Context& con, oui::Vector offset) const override
		{
			constexpr auto padding = Vector{ 2, 2 };
			const Rectangle lbox =
			{
				offset + _box.min() + Vector{ 0, -2 },
				offset + _box.max()
			};
			const Rectangle cbox =
			{
				offset + _float_box.min() - padding,
				offset + _float_box.max() + padding
			};

			const auto bend = Point{ cbox.min.x - 10, lbox.min.y+1 };

			oui::set(oui::Blend::multiply);
			oui::set(_color);
			oui::set(oui::LineThickness{ 2 });

			oui::fill(lbox);
			oui::fill(cbox);
			oui::line({ lbox.max.x, lbox.min.y + 1 }, bend);
			oui::line(bend, Point{ cbox.min.x, std::clamp(bend.y, cbox.min.y, cbox.max.y) });

			Group::render(con, offset + (_float_box.offset - _box.offset));
		}
	};

	class Comment : public Float
	{
	public:
		Comment(string) : Float({ 1, 0.8, 0.1 }) { }

		bool terminatedBy(string_view) const final { return false; }

		void enforceRules() final 
		{ 
			Float::enforceRules();
		}

		void serialize(std::ostream& out) const final
		{
			out << '%';
			Group::serialize(out);
			out << space_after;
		}
	};

	class Footnote : public Float
	{
		string _id;
		Font _font;
	public:
		Footnote(string) : Float(oui::colors::white) { }//Float({ 0.92, 0.98, 1 }) { }

		bool terminatedBy(string_view) const final { return false; }

		void enforceRules() final
		{
			Float::enforceRules();
		}

		Box& updateLayout(Context& con) final
		{
			Float::updateLayout(con);

			con.footnote += 1;
			_id = std::to_string(con.footnote);
			_font = { con.font_type, shift(con.font_size, -3) };

			_box.after = con.fontData(_font)->offset(_id, con.ptsize(_font));

			return _box;
		}
		void render(tex::Context& con, oui::Vector offset) const final
		{
			const auto em = con.ptsize(_font);
			con.fontData(_font)->drawLine(offset + _box.min() - oui::Vector{ 0, 0.2f*em },
				_id, oui::colors::black, em);

			con.fontData(_font)->drawLine(offset + contentBox().min() -  oui::Vector{ _box.width() + em*0.125f, em*0.2f }, 
				_id, oui::colors::black, em);
			Float::render(con, offset);
		}

		void serialize(std::ostream& out) const final
		{
			out << "\\footnote{";
			Group::serialize(out);
			out << '}' << space_after;
		}
	};

	class Cite : public Float
	{
		string _key;
		Font _font;
	public:
		Cite(string key) : Float(oui::Color{ 0.8, 1, 0.75 }), _key(key) { }

		bool terminatedBy(std::string_view) const final { return false; }

		Box& updateLayout(Context& con) final
		{
			while (!empty())
				pop_back();

			if (auto bib = bibliography())
			{
				if (auto match = bib->entry(_key))
				{
					if (auto author = match->tag("author"))
						tokenizeText(*author + " ");
					else
						tokenizeText("[No author field] ");

					if (auto title = match->tag("title"))
						tokenizeText(*title);
					else
						tokenizeText("[No title field]");
				}
				else
					tokenizeText("[No match in bibilography]");
			}
			else
				tokenizeText("[No bibliography]");


			Float::updateLayout(con);

			_font = { con.font_type, con.font_size };
			_box.after = con.fontData(_font)->offset("("+_key+")", con.ptsize(_font));

			return _box;
		}
		void render(tex::Context& con, oui::Vector offset) const final
		{
			const auto em = con.ptsize(_font);
			con.fontData(_font)->drawLine(offset + _box.min(), "("+_key+")", oui::colors::black, em);

			Float::render(con, offset);
		}

	};

	class Math : public Group
	{
	public:
		Math(string) { }

		bool terminatedBy(string_view token) const final { return token == "$"; }

		Box& updateLayout(Context& con) final
		{
			auto old_mode = con.mode.push(Mode::math);
			auto old_type = con.font_type.push(FontType::italic);

			Group::updateLayout(con);
			return _box;
		}

		void render(tex::Context& con, oui::Vector offset) const
		{
			oui::set(oui::Color{ 0.9f, 0.9f, 1.0f });
			oui::fill(absBox());
			
			Group::render(con, offset);
		}

		void serialize(std::ostream& out) const final
		{
			out << '$';
			Group::serialize(out);
			out << '$' << space_after;
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
			out << space_after;
		}
	};

	class Frac : public CommandGroup
	{
	public:
		using CommandGroup::CommandGroup;

		Box& updateLayout(Context& con) final
		{
			Expects(con.mode == Mode::math);
			const auto p = front().getArgument();
			const auto q = p->group.next()->getArgument();

			auto old_font_size = con.font_size.push(shift(con.font_size, -2));

			auto& pbox = p->updateLayout(con);
			auto& qbox = q->updateLayout(con);

			_box.width(std::max(pbox.width(), qbox.width()), align::min);
			_box.above = pbox.height() + con.ptsize()*0.05f;
			_box.below = qbox.height() + con.ptsize()*0.05f;

			pbox.offset = { (_box.width() - pbox.width())*0.5f, -pbox.below - (_box.above - pbox.height()) };
			qbox.offset = { (_box.width() - qbox.width())*0.5f, +qbox.above + (_box.below - pbox.height()) };

			return _box;
		}

		void render(tex::Context& con, oui::Vector offset) const final
		{
			Group::render(con, offset);
			oui::set(oui::colors::black);
			oui::fill(oui::align::centerLeft(oui::origo + offset + _box.offset).size({ _box.width(), 1 }));
		}
	};
	class VerticalGroup : public Group
	{
	protected:
		Box& _vertical_layout(Context& con)
		{
			float height = 0;
			for (auto&& sub : *this)
			{
				auto& sbox = sub.updateLayout(con);
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

		Box& updateLayout(Context& con) override
		{
			_box.width(con.width, align::center);
			_box.above = _box.below = 0;

			return _vertical_layout(con);
		}
	};

	class Root : public VerticalGroup
	{
		std::vector<oui::Vector> _line_max;
	public:
		using VerticalGroup::VerticalGroup;

		bool terminatedBy(string_view) const final { return false; }

		Bibliography* bibliography() const noexcept { return nullptr; }

		Box& updateLayout(Context& con) final
		{
			_line_max.clear();
			con.floats.clear();
			con.section = 0;
			con.subsection = 0;
			con.footnote = 0;
			con.lines = nullptr;

			const auto em = con.ptsize();
			con.float_width = std::min(con.width*0.3f, em * 12);
			const auto main_width = con.width - (con.float_width + em);



			_box.before = 0;
			_box.after = con.width;
			_box.above = _box.below = 0;

			{	// restore width after block
				auto old_width = con.width.push(main_width - em);

				float height = 0;
				float line_height = 0;
				float line_offset = em;
				for (auto&& sub : *this)
				{
					auto& sbox = sub.updateLayout(con);
					if (sub.flow() == Flow::vertical)
					{
						height += line_height;
						_line_max.push_back({ line_offset, height });
						line_height = 0;
						line_offset = em;
						const auto sub_align = sbox.before / sbox.width();
						sbox.offset = { (main_width - em)*sub_align + 0.5f*em, height + sbox.above };
						height += sbox.height();
						_line_max.push_back({ sbox.offset.x + sbox.after, height });
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
			}

			// pass 2: place floats
			Vector pen = { con.width - (con.float_width + 0.5f*em) , 0 };
			auto lm_it = _line_max.begin();
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

				while (lm_it != _line_max.end() && lm_it->y < pen.y - lbox.above)
					++lm_it;
				pen.x = lm_it->x;
				while (lm_it != _line_max.end() && lm_it->y < pen.y + lbox.below)
				{
					++lm_it;
					pen.x = std::max(pen.x, lm_it->x);
				}
				pen.x += em;

				sub->floatOffset(pen - loff);

				pen.y += sub->contentBox().height() + 0.5f*em;
			}
			con.floats.clear();

			return _box;
		}
	};

	class Document : public VerticalGroup
	{
		string _initial_space;
	public:
		using VerticalGroup::VerticalGroup;

		void enforceRules() final { _enforce_child_rules(); }

		Node* expand() final
		{
			if (empty())
				return this;
			if (auto tp = as<Text>(&front()); 
				tp && tp->text.empty() && !tp->space_after.empty())
			{
				_initial_space = move(tp->space_after);
				tp->removeFromGroup();
			}

			Par* prev_par = nullptr;
			while (auto p = prev_par ? prev_par->group.next() : &front())
			{
				p = p->expand();

				if (auto pp = as<Par>(p))
				{
					prev_par = pp;
					continue;
				}

				if (!prev_par
					|| prev_par->partype() != Par::Type::simple
					|| !prev_par->terminator.empty())
					prev_par = p->insertBeforeThis(intrusive::refcount::make<Par>("par"));
				prev_par->append(p->detachFromGroup());
			}

			return this;
		}

		bool terminatedBy(string_view token) const final { return token == "document"; }

		Bibliography* bibliography() const noexcept
		{
			for (auto&& e : *this)
				if (auto p = as<Par>(&e))
				for (auto&& pe : *p)
					if (auto bib = as<Bibliography>(&pe))
						return bib;
			return nullptr;
		}

		bool collect(Paragraph&) final { return false; }

		Box& updateLayout(Context& con) override
		{
			auto old_type = con.font_type.push(FontType::roman);
			auto old_width = con.width.push(std::min<float>(con.width, con.ptsize() * 24.0f));
			_box.width(con.width, align::center);
			_box.above = _box.below = 0;

			return _vertical_layout(con);
		}

		void serialize(std::ostream& out) const final
		{
			out << "\\begin{document}" << _initial_space;
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
		static constexpr frozen::unordered_map<string_view, Owner<Group>(*)(string), 12>
			maker_lookup =
		{
		{ "%", make_group<Comment> },
		{ "footnote", make_group<Footnote> },
		{ "math", make_group<Math> },
		{ "frac", make_group<Frac> },
		{ "par", make_group<Par> },
		{ "root", make_group<Root> },
		{ "document", make_group<Document> },
		{ "title", make_group<Par> },
		{ "author", make_group<Par> },
		{ "section", make_group<Par> },
		{ "subsection", make_group<Par> },
		{ "bibliography", make_group<Bibliography> }
		};

		return find(maker_lookup, name, default_value = &make_group<Curly>)(std::move(name));
	}

	static string read_optional_text(string_view data)
	{
		Expects(!data.empty());
		if (data.front() != '[')
			return {};

		if (const auto found = data.find_first_of(']', 1); found != data.npos)
			return string(data.substr(0, found + 1));

		throw IllFormed("could not find end of optional argument (only non-space text supported)");
	}
	static Owner<Node> read_optional(Node* next)
	{
		auto text = as<Text>(next);
		if (!text || text->text.empty())
			return {};

		if (text->text.front() != '[')
			return {};

		auto opt = read_optional_text(text->text);

		if (opt.empty())
			return {};

		if (opt.size() == text->text.size())
			return next->detachFromGroup();

		auto result = Text::make(move(opt));
		text->text.erase(0, result->text.size());
		return result;
	}


	using CommandExpander = Owner<Group>(*)(Command*);

	// pops mandatory, optional and then mandatory argument, no expansion
	Owner<Group> expand_aoa(Command* src)
	{
		Owner<Group> result = Group::make(src->cmd);
		tryPopArgument(src->group.next(), *result);
		if (auto opt = read_optional(src->group.next()))
			result->append(move(opt));
		tryPopArgument(src->group.next(), *result);
		return result;
	}
	// pops command plus and optional and a mandatory argument, no expansion
	Owner<Group> expand_coa(Command* src)
	{
		auto result = Group::make(src->cmd);
		result->append(Command::make(std::move(src->cmd)));
		if (auto opt = read_optional(src->group.next()))
			result->append(move(opt));
		tryPopArgument(src->group.next(), *result);
		return result;
	}

	// pops one argument and expands it
	Owner<Group> expand_A(Command* src)
	{
		auto result = Group::make(src->cmd);
		tryPopArgument(src->group.next(), *result); result->back().expand();
		return result;
	}
	// pops two arguments, expanding both
	Owner<Group> expand_AA(Command* src)
	{
		auto result = Group::make(src->cmd);
		tryPopArgument(src->group.next(), *result); result->back().expand();
		tryPopArgument(src->group.next(), *result); result->back().expand();
		return result;
	}

	// demand one curly after, and expand it
	Owner<Group> expand_C(Command* src)
	{
		auto result = Group::make(src->cmd);
		if (auto cp = as<Curly>(src->group.next()))
		{
			cp->expand();
			while (!cp->empty())
				result->append(cp->front().detachFromGroup());
			cp->detachFromGroup();
		}
		else
			throw IllFormed("missing { after \\", src->cmd);
		return result;
	}

	Owner<Group> expand_cite(Command* src)
	{
		if (auto cp = as<Curly>(src->group.next()))
		{
			auto key = as<Text>(&*cp->begin());
			Expects(key && !key->group.next());
			auto result = intrusive::refcount::make<Cite>(key->text);
			cp->removeFromGroup();
			return result;
		}
		else
			throw IllFormed("missing { after \\cite");

	}


	Node * Command::expand()
	{
		static constexpr frozen::unordered_map<string_view, CommandExpander, 8> cases =
		{
		//{ "newcommand", &expand_aoa },
		//{ "usepackage", &expand_coa },
		//{ "documentclass", &expand_coa },
		{ "frac", &expand_AA },
		{ "title", &expand_C },
		{ "author", &expand_C },
		{ "section", &expand_C },
		{ "subsection", &expand_C },
		{ "footnote", &expand_C },
		{ "bibliography", &expand_C },
		{ "cite", &expand_cite }
		};

		auto cmd_case = cases.find(cmd);
		if (cmd_case == cases.end())
			return this;

		auto result = cmd_case->second(this);

		const auto raw_result = result.get();
		const auto forget_self = replaceWith(move(result));
		return raw_result;
	}

}
