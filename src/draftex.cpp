#include <vector>
#include <string>
#include <string_view>
#include <variant>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <unordered_map>

#include <oui_debug.h>

#include "tex_node.h"

using tex::int_size;

using oui::utf8len;

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

std::string readFile(const std::string& filename)
{
	std::string result;
	std::ifstream file(filename, std::ios::binary);
	file.seekg(0, std::ios::end);
	if (!file)
		return result;
	const auto size = static_cast<size_t>(file.tellg());
	result.resize(size);
	file.seekg(0, std::ios::beg);
	file.read(result.data(), result.size());
	return result;
}

static auto subview(std::string_view text, size_t off, size_t count) { return text.substr(off, count); }


inline auto is_above(const tex::Node& node)
{ 
	return [&node](const tex::Node& other)
	{ 
		return other.absBottom() <= node.absTop(); 
	};
}
inline auto is_below(const tex::Node& node)
{
	return [&node](const tex::Node& other)
	{
		return other.absTop() >= node.absBottom();
	};
}

struct Caret
{
	static constexpr float no_target = std::numeric_limits<float>::quiet_NaN();

	tex::Text* node = nullptr;
	int offset = 0;
	float target_x = no_target;
	bool change = false;

	enum class Move : char { backward, forward };


	int maxOffset() const { Expects(node); return node->text.size(); }

	float offsetX(tex::Context& con) const
	{
		return con.font(node->font)->offset(subview(node->text, 0, offset), con.ptsize(node->font));
	}

	void render(tex::Context& con)
	{
		if (!con.window().focus())
			return;

		auto box = node->absBox();
	

		box.min.x += offsetX(con) - 1;
		box.max.x = box.min.x + 2;

		oui::fill(box, oui::colors::black);
	}

	int repairOffset(int off)
	{
		Expects(node && off < node->text.size());
		while (off > 0 && utf8len(node->text[off]) == 0)
			--off;
		return off;
	}

	void prepare(Move move)
	{
		if (node->text.empty())
		{
			if (space(node->prev()) && space(node->next()))
			{
				move == Move::forward ?
					erasePrev() :
					eraseNext();
			}
			if (!node->prev() && !node->next() && typeid(*node->parent) == typeid(tex::Par))
			{
				const auto to_remove = node->parent();
				if (move == Move::forward)
				{
					if (const auto candidate = node->prevText())
						node = candidate;
					else
						return;
					offset = node->text.size();
				}
				else
				{
					if (const auto candidate = node->nextText())
						node = candidate;
					else
						return;
					offset = 0;
				}
				to_remove->remove();
			}
		}
	}

	void next()
	{
		if (!node)
			return;
		prepare(Move::forward);
		target_x = no_target;

		if (offset < node->text.size())
		{
			offset += utf8len(node->text[offset]);
			return;
		}
		if (auto next_text = node->nextText())
		{
			node = next_text;
			offset = 0;
			return;
		}
		return;
	}
	void prev()
	{
		if (!node)
			return;
		prepare(Move::backward);
		target_x = no_target;

		if (offset > 0)
		{
			offset = repairOffset(offset-1);
			return;
		}
		if (auto prev_text = node->prevText())
		{
			node = prev_text;
			offset = node->text.size();

			return;
		}
		return;
	}

	void findPlace(tex::Context& con)
	{
		const auto font = con.font(node->font);
		const auto textdata = std::string_view(node->text);
		auto prev_x = node->absLeft();

		for (int i = 0, len = 1; i < int_size(textdata); i += len)
		{
			len = utf8len(node->text[i]);
			const auto x = prev_x + font->offset(textdata.substr(i, len), con.ptsize(node->font));
			if (x >= target_x)
			{
				offset = x - target_x > target_x - prev_x ? i : i + len;
				return;
			}
			prev_x = x;
		}
		offset = int_size(textdata);
	}

	void up(tex::Context& con)
	{
		using namespace tex;
		using namespace xpr;

		if (!node)
			return;
		prepare(Move::backward);

		if (isnan(target_x))
			target_x = node->absLeft() + offsetX(con);

		if (auto first_above = first.of(those.of(node->allTextBefore()).that(is_above(*node)))) 
		{
			if (target_x >= first_above->absLeft())
			{
				node = first_above;
				return findPlace(con);
			}
			Text* prev_above = first_above;
			for (auto&& above : first_above->allTextBefore())
			{
				if (is_above(*prev_above)(above))
					break;
				if (target_x >= above.absLeft())
				{
					node = (target_x - above.absRight() < prev_above->absLeft() - target_x) ? 
						&above : prev_above;
					return findPlace(con);
				}
				prev_above = &above;
			}
			node = prev_above;
			return findPlace(con);
		}
	}
	void down(tex::Context& con)
	{
		using namespace tex;
		using namespace xpr;

		if (!node)
			return;
		prepare(Move::forward);

		if (isnan(target_x))
			target_x = node->absLeft() + offsetX(con);

		if (auto first_below = first.of(those.of(node->allTextAfter()).that(is_below(*node))))
		{
			if (target_x <= first_below->absRight())
			{
				node = first_below;
				return findPlace(con);
			}
			auto prev_below = first_below;
			for (auto&& below : first_below->allTextAfter())
			{
				if (is_below(*prev_below)(below))
					break;
				if (target_x <= below.absRight())
				{
					node = (target_x - below.absLeft() > prev_below->absRight() - target_x) ?
						&below : prev_below;
					return findPlace(con);
				}
				prev_below = &below;
			}
			node = prev_below;
			return findPlace(con);
		}
	}
	void home()
	{
		prepare(Move::backward);
		target_x = no_target;
		while (auto prev_text = node->prevText())
		{
			if (prev_text->absLeft() > node->absLeft())
			{
				offset = 0;
				return;
			}
			node = prev_text;
		}
	}
	void end()
	{
		prepare(Move::forward);
		target_x = no_target;
		while (auto next_text = node->nextText())
		{
			if (next_text->absRight() < node->absRight())
			{
				offset = maxOffset();
				return;
			}
			node = next_text;
		}
	}

	void eraseNext()
	{
		target_x = no_target;
		if (offset >= maxOffset())
		{
			if (node->next)
			{
				Expects(not text(*node->next));
				node->next->remove();
				if (auto tnext = tex::as<tex::Text>(node->next()))
				{
					node->text.append(tnext->text);
					node->next->remove();
				}
			}
			return;
		}
		node->change();
		node->text.erase(offset, utf8len(node->text[offset]));
		if (node->text.empty())
		{
			//handle_empty();
		}
	}
	void erasePrev()
	{
		target_x = no_target;
		if (offset <= 0)
		{
			if (node->prev)
			{
				Expects(not text(*node->prev));
				node->prev->remove();
				if (auto tprev = tex::as<tex::Text>(node->prev()))
				{
					offset = tprev->text.size();
					node->text.insert(0, tprev->text);
					node->prev->remove();
				}
			}
			return;
		}
		prev();
		eraseNext();
	}

	void insertSpace()
	{
		if (offset == 0 && nullOrSpace(node->prev()))
			return;
		const auto space = node->insertSpace(offset);
		node = space->nextText();
		offset = 0;
	}

	void breakParagraph()
	{
		Expects(node);
		if (typeid(*node->parent) != typeid(tex::Par))
			return;
		if (offset == 0 && !node->parent->contains(node->prevText()))
			return;

		auto new_par = node->parent->insertAfter(tex::Group::make("par"));
		auto old_node = node;

		if (offset <= 0)
		{
			old_node = node->insertBefore(tex::Text::make());
			new_par->append(node->detach());
		}
		else if (offset >= node->text.size())
			node = new_par->append(tex::Text::make());
		else
		{
			node = new_par->append(tex::Text::make(node->text.substr(offset)));
			old_node->text.resize(offset);
		}
		while (old_node->next())
			new_par->append(old_node->next->detach());

		offset = 0;
	}

	void nextStop()
	{
		if (auto new_node = node->nextStop())
		{
			node = new_node;
			offset = 0;
		}
	}
	void prevStop()
	{
		if (auto new_node = node->prevStop())
		{
			node = new_node;
			offset = node->text.size();
		}
	}
};

struct Draftex;

using Action = void(Draftex::*)();

struct Option
{
	std::string_view name;
	Action action = nullptr;
	const Option* subs = nullptr;
	char subc = 0;
	char highlight;
	oui::Key key;

	constexpr Option(std::string_view name, char highlight, oui::Key key, Action action) :
		name(name), action(action), highlight(highlight), key(key) { }
	template <char N>
	constexpr Option(std::string_view name, char highlight, oui::Key key, const Option (&suboptions)[N]) :
		name(name), subs(suboptions), subc(N), highlight(highlight), key(key) { }
};

namespace menu
{
	extern const Option main[4];
}

struct Draftex
{

	oui::Window window;
	tex::Context context;
	bool shift_down = false;

	tex::Owner<tex::Group> tokens;

	decltype(xpr::those.of(menu::main)) options = { nullptr, nullptr };
	Caret caret{ nullptr, 0 };
	volatile bool ignore_char = false;

	Draftex() : window{ { "draftex", 1280, 720, 8 } }, context(window)
	{
		tokens = tex::tokenize(readFile("test.tex"));
		tokens->expand();
		tokens->enforceRules();
		check_title();

		for (auto&& e : *tokens)
			if (auto group = tex::as<tex::Group>(&e); group && group->terminatedBy("document"))
			{
				if (group->prev)
					caret.node = group->prev->nextText();
				else
					caret.node = group->parent->nextText();
				break;
			}

		window.resize = [this](auto&&) { tokens->change(); };
		oui::input.keydown = [this](oui::Key key, auto prev_state)
		{
			const bool is_repeat = prev_state == oui::PrevKeyState::down;
			ignore_char = false;
			using oui::Key;

			for (auto&& opt : options)
				if (opt.key == key)
				{
					if (opt.subs)
					{
						options = { opt.subs, opt.subs + opt.subc };
						window.redraw();
					}
					else if (opt.action)
					{
						take_option();
						(this->*opt.action)();
					}
					return;
				}
			

			switch (key)
			{
			case Key::shift: shift_down = true; break;
			case Key::home:  caret.home(); break;
			case Key::end:   caret.end();  break;
			case Key::right: caret.next(); break;
			case Key::left:  caret.prev(); break;
			case Key::up:     caret.up(context); break;
			case Key::down: caret.down(context); break;
			case Key::backspace: caret.erasePrev(); break;
			case Key::del:       caret.eraseNext(); break;
			case Key::space:     caret.insertSpace(); break;
			case Key::enter:     caret.breakParagraph(); break;
			case Key::tab:
				shift_down ? 
					caret.prevStop() : 
					caret.nextStop();
				break;
			case Key::alt: case Key::f10:
				if (!is_repeat) 
					toggle_menu();
				break;
			default:
				oui::debug::println("key ", static_cast<int>(key));
				return;
			}
			window.redraw();
		};
		oui::input.keyup = [this](oui::Key key)
		{
			if (key == oui::Key::shift)
				shift_down = false;
		};
		oui::input.character = [this](int charcode)
		{
			if (charcode <= ' ')
				return;
			if (!options.empty())
				return;
			if (ignore_char)
			{
				ignore_char = false;
				return;
			}

			caret.offset += caret.node->insert(caret.offset, oui::utf8(charcode));
			caret.target_x = Caret::no_target;
			window.redraw();
			return;
		};


	}

	void take_option()
	{
		options = { nullptr, nullptr };
		window.redraw();
		ignore_char = true;
	}

	void quit() { window.close(); }
	void save()
	{
		take_option();
		tokens->serialize(std::ofstream("test.out"));
	}
	void insert_group(std::string_view group_type)
	{
		auto result = tex::Group::make(group_type);
		if (caret.offset >= caret.node->text.size())
		{
			caret.offset = 0;
			caret.node = caret.node
				->insertAfter(move(result))
				->append(tex::Text::make());
			return;
		}
		if (caret.offset > 0)
		{
			auto prev_node = caret.node;
			caret.node = caret.node->insertAfter(tex::Text::make(caret.node->text.substr(caret.offset)));
			prev_node->text.resize(caret.offset);
			caret.offset = 0;
		}
		caret.node = caret.node
			->insertBefore(move(result))
			->append(tex::Text::make());
	}

	void insert_math()
	{
		insert_group("math");
	}
	void insert_comment()
	{
		insert_group("%");
	}

	void change_par(tex::Par::Type new_type)
	{
		for (const tex::Node* n = caret.node; ; n = n->parent())
			if (auto par = tex::as<tex::Par>(n->parent()))
			{
				par->partype(new_type);
				return;
			}
		//TODO: maybe warn if no para parent found?
	}
	template <tex::Par::Type NewType>
	void change_par()
	{
		change_par(NewType);
	}

	void toggle_menu()
	{
		window.redraw();
		if (!options.empty())
		{
			options = { nullptr, nullptr };
			return;
		}
		options = xpr::those.of(menu::main);
	}

	void check_title() 
	{
		for (auto& re : *tokens)
			if (auto g = tex::as<tex::Group>(&re); g && g->terminatedBy("document"))
				if (auto doc = dynamic_cast<tex::Group*>(&re))
					for (auto& de : *doc)
						if (auto p = tex::as<tex::Par>(&de); p && p->partype() == tex::Par::Type::title)
						{
							std::ostringstream os;
							for (auto& te : *p)
								te.serialize(os);
							window.title("draftex " + os.str());
							return;
						}
		window.title("draftex");
	}

	void render()
	{
		window.clear(oui::colors::white);
		context.reset(window);
		context.keysize = 9 * window.dpi() / 72.0f;
		if (tokens->changed())
		{
			tokens->enforceRules();
			auto& tbox = tokens->updateSize(context,
				tex::Mode::text, { tex::FontType::sans, tex::FontSize::normalsize },
				window.area().width());
			tokens->layoutOffset({ window.area().width()*tbox.before/tbox.width(), 0 });
			tokens->commit();
			check_title();
		}

		const auto caret_box = caret.node->absBox();
		const auto shift = oui::Vector{ 0, window.area().height()*0.5f - caret_box.center().y };
	
		oui::shift(shift);

		oui::fill(caret_box, { 0.0f, 0.1f, 1, 0.2f });
		for (auto p = caret.node->parent(); p != nullptr; p = p->parent())
			if (tex::as<tex::Par>(p))
				oui::fill(p->absBox(), { 0, 0, 1, 0.1f });

		tokens->render(context, {});

		caret.render(context);

		oui::shift(-shift);

		if (!options.empty())
		{
			oui::fill(window.area(), oui::Color{ 1,1,1,0.3f });
			auto optfont = context.font(tex::FontType::sans);

			const float h = 24;

			oui::Point pen = { 0, 0 };
			for (auto&& opt : options)
			{
				optfont->drawLine(pen, opt.name, oui::colors::black, h);
				oui::Point underline_tlc
				{
					pen.x + optfont->offset(opt.name.substr(0, opt.highlight), h),
					pen.y + h * 0.85f
				};
				oui::fill(oui::align::topLeft(underline_tlc)
					.size({ optfont->offset(opt.name.substr(opt.highlight, 1), h), h*0.0625f }), oui::colors::black);
				pen.y += h;
			}
		}
	}
};

namespace menu
{
	using Key = oui::Key;

	const Option file[] =
	{
		{ "Save", 0, Key::s, &Draftex::save },
		{ "Exit", 1, Key::x, &Draftex::quit }
	};

	using Par = tex::Par::Type;

	const Option par[] =
	{
	{ "Standard", 0, Key::s, &Draftex::change_par<Par::simple> },
	{ "Title",    0, Key::t, &Draftex::change_par<Par::title> },
	{ "Author",   0, Key::a, &Draftex::change_par<Par::author> },
	{ "Heading 2 - Section",    8, Key::n2, &Draftex::change_par<Par::section> },
	{ "Heading 3 - Subsection", 8, Key::n3, &Draftex::change_par<Par::subsection> }
	};

	const Option math[] =
	{
		{ "Insert", 0, Key::i, &Draftex::insert_math }
	};

	const Option main[] =
	{
		{ "File",      0, Key::f, file },
		{ "Paragraph", 0, Key::p, par },
		{ "Math",      0, Key::m, math },
		{ "Comment",   0, Key::c, &Draftex::insert_comment }
	};

}



int main()
{
	oui::debug::println("sizeof Node: ", sizeof(tex::Node));

	Draftex state;
	while (state.window.update())
		state.render();

	//tokens->serialize(std::ofstream("test.out"));

	return 0;
}
