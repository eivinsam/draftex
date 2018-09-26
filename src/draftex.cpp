#include <fstream>
#include <sstream>

#include <oui_debug.h>

#include "file_mapping.h"

#include "edit.h"

using std::move;

using oui::utf8len;

using namespace tex;

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

class History
{
	edit::Stack<uptr<edit::Action>> _undo;
	edit::Stack<uptr<edit::Action>> _redo;
public:

	void add(uptr<edit::Action> a)
	{ 
		if (!a)
			return;
		uptr<edit::Action> combo;
		if (!_undo.empty())
			combo = edit::combine(*a, *_undo.peek());
		
		if (combo)
		{
			(void)_undo.pop();
			if (!as<edit::Annihilation>(combo.get()))
				_undo.push(move(combo));
		}
		else
			_undo.push(move(a)); 
		_redo.clear(); 
	}

	void undo(Caret& caret) 
	{ 
		if (!_undo.empty())
		{
			auto[redo, new_caret] = _undo.pop()->perform();
			_redo.push(move(redo));
			caret = new_caret;
		}
	}
	void redo(Caret& caret) 
	{ 
		if (!_redo.empty())
		{
			auto[undo, new_caret] = _redo.pop()->perform();
			_undo.push(move(undo));
			caret = new_caret;
		}
	}
};

using intrusive::as_mutable;

struct Draftex
{
	oui::Window window;
	tex::Context context;

	tex::Owner<tex::Group> tokens;

	History history;

	decltype(xpr::those.of(menu::main)) options = { nullptr, nullptr };
	Caret caret{ nullptr, 0 };
	volatile bool ignore_char = false;

	Draftex() : window{ { "draftex", 1280, 720, 8 } }, context(window)
	{
		tokens = tex::tokenize(FileMapping("test.tex").data);
		tokens->expand();
		tokens->enforceRules();
		tokens->apply(refreshCites);
		check_title();

		for (auto&& e : *tokens)
			if (auto group = tex::as<tex::Group>(&e); group && group->terminatedBy("document"))
			{
				if (group->group.prev())
					caret.node = group->group.prev()->nextText();
				else
					caret.node = group->group->nextText();
				caret.resetStart();
				break;
			}

		window.resize = [this](auto&&) { tokens->markChange(); };
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
			case Key::alt: case Key::f10:
				if (!is_repeat)
					toggle_menu();
				return;
			case Key::escape:
				if (!is_repeat && !options.empty())
					toggle_menu();
				return;
			default:
				if (!options.empty())
					return;
				break;
			}

			switch (key)
			{
			case Key::home:  history.add(caret.home());        break;
			case Key::end:   history.add(caret.end());         break;
			case Key::right: history.add(caret.next());        break;
			case Key::left:  history.add(caret.prev());        break;
			case Key::up:    history.add(caret.up(context));   break;
			case Key::down:  history.add(caret.down(context)); break;
			case Key::backspace: history.add(caret.erasePrev()); break;
			case Key::del:       history.add(caret.eraseNext()); break;
			case Key::space:     history.add(caret.insertSpace()); break;
			case Key::enter:     caret.breakParagraph(); break;
			case Key::tab:
				oui::pressed(Key::shift) ? 
					caret.prevStop() : 
					caret.nextStop();
				break;
			case Key::z:
				if (oui::pressed(Key::ctrl))
				{
					oui::pressed(Key::shift) ?
						history.redo(caret) :
						history.undo(caret);
					ignore_char = true;
				}
				break;
			default:
				oui::debug::println("key ", static_cast<int>(key));
				return;
			}
			if (!oui::pressed(Key::shift))
				caret.resetStart();

			window.redraw();
		};
		//oui::input.keyup = [this](oui::Key key)
		//{
		//};
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

			auto text = oui::utf8(charcode);

			history.add(caret.perform<edit::InsertText>(claim_mutable(caret.node), caret.offset, text, Caret::Move::forward));

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
		std::ofstream out("test.out");
		tokens->serialize(out);
	}
	void insert_group(std::string_view /*group_type*/)
	{
		//auto result = tex::Group::make(group_type);
		//if (caret.offset >= caret.node->text.size())
		//{
		//	caret.offset = 0;
		//	caret.node = caret.node
		//		->insertAfterThis(move(result))
		//		->append(tex::Text::make());
		//	return;
		//}
		//if (caret.offset > 0)
		//{
		//	auto prev_node = caret.node;
		//	caret.node = caret.node->insertAfterThis(tex::Text::make(caret.node->text.substr(caret.offset)));
		//	prev_node->text.resize(caret.offset);
		//	caret.offset = 0;
		//}
		//caret.node = caret.node
		//	->insertBeforeThis(move(result))
		//	->append(tex::Text::make());
	}

	void insert_math()
	{
		insert_group("math");
	}
	void insert_comment()
	{
		insert_group("%");
	}

	void change_par(tex::Par::Type /*new_type*/)
	{
		//for (tex::Node* n = caret.node; ; n = n->group())
		//	if (auto par = tex::as<tex::Par>(n->group()))
		//	{
		//		par->partype(new_type);
		//		return;
		//	}
		////TODO: maybe warn if no para parent found?
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
							window.title("draftex - " + os.str());
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
			auto set_width = context.width.push(window.area().width());
			const auto& tbox = tokens->updateLayout(context);
			tokens->layoutOffset({ window.area().width()*tbox.before/tbox.width(), 0 });
			tokens->commit();
			check_title();
		}

		const auto caret_box = caret.node->absBox();
		const auto shift = oui::Vector{ 0, window.area().height()*0.5f - caret_box.center().y };
	
		oui::shift(shift);

		oui::set(oui::Color{ 0.0f, 0.1f, 1, 0.2f });
		oui::fill(caret_box);
		//for (auto p = caret.node->group(); p != nullptr; p = p->group())
		//	if (tex::as<tex::Par>(p))
		//		oui::fill(p->absBox(), { 0, 0, 1, 0.1f });

		tokens->render(context, {});

		caret.render(context);

		oui::shift(-shift);

		if (!options.empty())
		{
			oui::set(oui::Blend::normal);
			oui::set(oui::Color{ 1,1,1,0.3f });
			oui::fill(window.area());
			const auto optfont = context.fontData(tex::FontType::sans);

			const float h = 24;

			oui::set(oui::colors::black);
			oui::Point pen = { 0, 0 };
			for (auto&& opt : options)
			{
				optfont->drawLine(pen, opt.name, oui::colors::black, h);
				const oui::Point underline_tlc
				{
					pen.x + optfont->offset(opt.name.substr(0, opt.highlight), h),
					pen.y + h * 0.85f
				};
				oui::fill(oui::align::topLeft(underline_tlc)
					.size({ optfont->offset(opt.name.substr(opt.highlight, 1), h), h*0.0625f }));
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
