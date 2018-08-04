#include <vector>
#include <string>
#include <string_view>
#include <variant>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include <oui_debug.h>

#include "tex_node.h"

using oui::utf8len;

using tex::int_size;

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


	int maxOffset() const { Expects(node); return int_size(node->data); }

	float offsetX(tex::Context& con) const
	{
		return con.font(node->font)->offset(subview(node->data, 0, offset), con.ptsize(node->font));
	}

	void render(tex::Context& con)
	{
		if (!con.window().focus())
			return;

		oui::Point pos = node->absBox().min;
		pos.x += offsetX(con) - 1;

		oui::fill(oui::align::topLeft(pos).size({ 2.0f, node->box.height() }), oui::colors::black);
	}

	int repairOffset(int off)
	{
		Expects(node && off < int_size(node->data));
		while (off > 0 && utf8len(node->data[off]) == 0)
			--off;
		return off;
	}

	void prepare(Move move)
	{
		if (node->data.empty() && space(node->prev) && space(node->next))
		{
			move == Move::forward ?
				erasePrev() :
				eraseNext();
		}
	}

	void next()
	{
		if (!node)
			return;
		prepare(Move::forward);
		target_x = no_target;

		if (offset < int_size(node->data))
		{
			offset += utf8len(gsl::at(node->data, offset));
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
			offset = node->data.size();

			return;
		}
		return;
	}

	void findPlace(tex::Context& con)
	{
		const auto font = con.font(node->font);
		const auto textdata = std::string_view(node->data);
		auto prev_x = node->absLeft();

		for (int i = 0, len = 1; i < int_size(textdata); i += len)
		{
			len = utf8len(node->data[i]);
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
				if (text(node->next))
				{
					node->data.append(node->next->data);
					node->next->remove();
				}
			}
			return;
		}
		node->change();
		node->data.erase(offset, utf8len(gsl::at(node->data, offset)));
		if (node->data.empty())
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
				if (text(node->prev))
				{
					offset = int_size(node->prev->data);
					node->data.insert(0, node->prev->data);
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
		if (offset == 0 && nullOrSpace(node->prev))
			return;
		const auto space = node->insertSpace(offset);
		node = space->nextText();
		offset = 0;
	}
};

struct Draftex;

using Action = void(Draftex::*)();

struct Option
{
	std::string_view name;
	Action action = nullptr;
	const Option* subs = nullptr;
	short subc = 0;
	oui::Key key;

	constexpr Option(std::string_view name, oui::Key key, Action action) :
		name(name), action(action), key(key) { }
	template <short N>
	constexpr Option(std::string_view name, oui::Key key, const Option (&suboptions)[N]) :
		name(name), subs(suboptions), subc(N), key(key) { }
};

extern const Option main_menu[2];

struct Draftex
{
	oui::Window window;
	tex::Context context;

	tex::Owner<tex::Group> tokens;

	decltype(xpr::those.of(main_menu)) options = { nullptr, nullptr };
	Caret caret{ nullptr, 0 };
	volatile bool ignore_char = false;

	Draftex() : window{ { "draftex", 1280, 720, 8 } }, context(window)
	{
		tokens = tex::tokenize(readFile("test.tex"));
		tokens->expand();
		check_title();

		for (auto&& e : *tokens)
			if (auto group = tex::as<tex::Group>(&e); group && group->data == "document")
			{
				if (group->prev)
					caret.node = group->prev->nextText();
				else
					caret.node = group->parent->nextText();
				break;
			}

		window.resize = [this](auto&&) { tokens->changed(); };
		oui::input.keydown = [this](oui::Key key)
		{
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
			case Key::home:  caret.home(); break;
			case Key::end:   caret.end();  break;
			case Key::right: caret.next(); break;
			case Key::left:  caret.prev(); break;
			case Key::up:     caret.up(context); break;
			case Key::down: caret.down(context); break;
			case Key::backspace: caret.erasePrev(); break;
			case Key::del:       caret.eraseNext(); break;
			case Key::space:     caret.insertSpace(); break;
			case Key::alt:       toggle_menu(); break;
			default:
				oui::debug::println("key " + std::to_string(static_cast<int>(key)));
				return;
			}
			window.redraw();
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
	void insert_math()
	{
		if (caret.offset >= int_size(caret.node->data))
		{
			caret.offset = 0;
			caret.node = caret.node
				->insertAfter(tex::Group::make("math"))
				->append(tex::Text::make());
			return;
		}
		if (caret.offset > 0)
		{
			caret.offset = 0;
			caret.node = caret.node->insertAfter(tex::Text::make(caret.node->data.substr(caret.offset)));
			caret.node->prev->data.resize(caret.offset);
		}
		caret.node = caret.node
			->insertBefore(tex::Group::make("math"))
			->append(tex::Text::make());
	}

	void toggle_menu()
	{
		window.redraw();
		if (!options.empty())
		{
			options = { nullptr, nullptr };
			return;
		}
		options = xpr::those.of(main_menu);
	}

	void check_title() 
	{
		for (auto& re : *tokens)
			if (re.data == "document")
				if (auto doc = dynamic_cast<tex::Group*>(&re))
					for (auto& de : *doc)
						if (de.data == "title")
							if (auto title = dynamic_cast<tex::Group*>(&de))
							{
								std::ostringstream os;
								for (auto& te : *title)
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
			tokens->updateSize(context,
				tex::Mode::text, { tex::FontType::sans, tex::FontSize::normalsize },
				window.area().width());
			tokens->updateLayout({ window.area().width()*0.5f, 0 });
			tokens->commit();
			check_title();
		}

		oui::fill(caret.node->absBox(), { 0.0f, 0.1f, 1, 0.2f });
		for (tex::Group* p = caret.node->parent; p != nullptr; p = p->parent)
			if (p->data == "par")
				oui::fill(p->absBox(), { 0, 0, 1, 0.1f });

		tokens->render(context, {});

		caret.render(context);

		if (!options.empty())
		{
			oui::fill(window.area(), oui::Color{ 1,1,1,0.3f });
			auto optfont = context.font(tex::FontType::sans);

			const float h = 24;

			oui::Point pen = { 0, 0 };
			for (auto&& opt : options)
			{
				optfont->drawLine(pen, opt.name, oui::colors::black, h);
				pen.y += h;
			}
		}
	}
};

const Option file_menu[] = 
{
	{ "Save", oui::Key::s, &Draftex::save },
	{ "Exit", oui::Key::x, &Draftex::quit }
};

const Option math_menu[] =
{
	{ "Insert", oui::Key::i, &Draftex::insert_math }
};

const Option main_menu[] =
{
	{ "File", oui::Key::f, file_menu },
	{ "Math", oui::Key::m, math_menu }
};

int main()
{
	Draftex state;
	while (state.window.update())
		state.render();

	//tokens->serialize(std::ofstream("test.out"));

	return 0;
}
