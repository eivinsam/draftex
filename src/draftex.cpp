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
#include "file_mapping.h"

using std::move;

using tex::int_size;

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

template <class T>
class Stack
{
	std::vector<T> _data;
public:

	bool empty() const { return _data.empty(); }

	void push(T&& value) { _data.push_back(std::move(value)); }
	void push(const T& value) { _data.push_back(value); }

	T& peek() { return _data.back(); }
	const T& peek() const { return _data.back(); }

	T pop() { auto result = move(_data.back()); _data.pop_back(); return result; }

	void clear() { _data.clear(); }
};

template <class T, class D = std::default_delete<T>>
using uptr = std::unique_ptr<T, D>;

class Reaction
{
public:
	virtual ~Reaction() { }
	virtual uptr<Reaction> perform() = 0;
};

template <class T>
Owner<T> claim(T* ptr) { return intrusive::refcount::claim(ptr); }


template <class A>
class Do : public Reaction, public A
{
public:
	template <class... Args>
	Do(Args&&... args) : A{ std::forward<Args>(args)... } { }

	uptr<Reaction> perform() final { return A::perform(); }
};

template <class A, class... Args>
uptr<Do<A>> make_action(Args&&... a)
{
	return std::make_unique<Do<A>>(std::forward<Args>(a)...);
}

struct RemoveText
{
	Owner<Text> node;
	int offset;
	int length;

	uptr<Reaction> perform();
};
struct InsertText
{
	Owner<Text> node;
	int offset;
	string text;

	uptr<Reaction> perform();
};
uptr<Reaction> RemoveText::perform()
{
	auto redo = make_action<InsertText>(node, offset, node->text.substr(offset, length));
	node->text.erase(offset, length);
	node->change();
	return redo;
}
uptr<Reaction> InsertText::perform()
{
	auto redo = make_action<RemoveText>(node, offset, text.size());
	node->text.insert(offset, text);
	node->change();
	return redo;
}

struct RemoveSpace
{
	Owner<Node> node;

	uptr<Reaction> perform();
};
struct InsertSpace
{
	Owner<Node> node;
	string space;

	uptr<Reaction> perform();
};
uptr<Reaction> RemoveSpace::perform()
{
	auto redo = make_action<InsertSpace>(node, node->space_after);
	node->space_after = "";
	node->change();
	return redo;
}
uptr<Reaction> InsertSpace::perform()
{
	auto redo = make_action<RemoveSpace>(node);
	node->space_after = space;
	node->change();
	return redo;
}

struct Caret
{
	static constexpr float no_target = std::numeric_limits<float>::quiet_NaN();

	enum class Move : char { none, backward, forward };

	tex::Text* node = nullptr;
	tex::Text* node_start = nullptr;
	int offset = 0;
	int offset_start = 0;
	float target_x = no_target;
	bool change = false;

	constexpr bool hasSelection() const { return offset != offset_start || node != node_start; }


	void resetStart()
	{ 
		node_start = node; 
		offset_start = offset; 
	}

	int maxOffset() const { Expects(node); return node->text.size(); }


	static float offsetXof(tex::Context& con, tex::Text* n, int o)
	{
		return con.fontData(n->font)->offset(subview(n->text, 0, o), con.ptsize(n->font));
	}

	float offsetX(tex::Context& con) const
	{
		return offsetXof(con, node, offset);
	}

	void render(tex::Context& con)
	{
		if (!con.window().focus())
			return;

		auto render_one = [&con](tex::Text* n, int o)
		{
			auto box = n->absBox();
			box.min.x += offsetXof(con, n, o) - 1;
			box.max.x = box.min.x + 2;
			oui::fill(box);
		};


		oui::set(oui::Blend::multiply);
		oui::set(oui::colors::black);
		render_one(node, offset);


		oui::set(oui::Color{ 0.9,1,0.5 });

		const auto xe = offsetX(con);
		const auto xs = offsetXof(con, node_start, offset_start);

		if (node == node_start)
		{
			auto box = node->absBox();

			oui::fill({ {box.min.x + xs, box.min.y}, {box.min.x + xe, box.max.y } });

			return;
		}

		auto to_mark = interval(*node_start, *node);
		Expects(to_mark.size() >= 2);
		const auto fwd = to_mark.front() == node_start;

		{
			auto box = node->absBox();
			(fwd ? box.max.x : box.min.x) = box.min.x + xe;
			oui::fill(box);
			box = node_start->absBox();
			(fwd ? box.min.x : box.max.x) = box.min.x + xs;
			oui::fill(box);
		}

		for (auto it = ++to_mark.begin(), end = --to_mark.end(); it != end; ++it)
			oui::fill((*it)->absBox());
	}

	int repairOffset(int off)
	{
		Expects(node && off < node->text.size());
		while (off > 0 && utf8len(node->text[off]) == 0)
			--off;
		return off;
	}

	void check_for_deletion(tex::Text& n)
	{
		const auto rem_node = [](tex::Node& n)
		{
			if (auto prev = n.group.prev();
				prev && prev->space_after.empty())
				prev->space_after = std::move(n.space_after);
			n.group->change();
			n.removeFromGroup();
		};

		if (&n == node)
			return;

		if (n.text.empty())
		{
			auto g = n.group();
			rem_node(n);
			while (g->empty())
				rem_node(*std::exchange(g, g->group()));
		}

	}

	void prepare(Move /*move*/)
	{
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
			check_for_deletion(*std::exchange(node, next_text));
			offset = 0;
			return;
		}
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
			check_for_deletion(*std::exchange(node, prev_text));
			offset = node->text.size();
			return;
		}
	}

	void findPlace(tex::Context& con)
	{
		const auto font = con.fontData(node->font);
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

	void findClosestOnLine(tex::Context& con, tex::Line* line)
	{
		if (!line)
			return;

		auto& original_node = *node;

		float closest_d = std::numeric_limits<float>::infinity();
		for (auto&& e : *line)
		{
			auto abs_box = e.absBox();
			float d = std::min(std::abs(target_x - abs_box.min.x), std::abs(target_x - abs_box.max.x));
			if (closest_d > d)
			{
				closest_d = d;
				node = &e;
			}
		}
		check_for_deletion(original_node);
		return findPlace(con);
	}

	void up(tex::Context& con)
	{
		if (!node)
			return;
		prepare(Move::backward);

		if (isnan(target_x))
			target_x = node->absLeft() + offsetX(con);
		if (node->line())
			findClosestOnLine(con, node->line->next());
	}
	void down(tex::Context& con)
	{
		if (!node)
			return;
		prepare(Move::forward);

		if (isnan(target_x))
			target_x = node->absLeft() + offsetX(con);
		if (node->line())
			findClosestOnLine(con, node->line->prev());
	}
	void home()
	{
		prepare(Move::backward);
		target_x = no_target;

		if (node->line())
		{
			check_for_deletion(*std::exchange(node, &*node->line->begin()));
			offset = 0;
		}
	}
	void end()
	{
		prepare(Move::forward);
		target_x = no_target;

		if (node->line())
		{
			check_for_deletion(*std::exchange(node, &*node->line->rbegin()));
			offset = node->text.size();
		}
	}

	uptr<Reaction> eraseSelection()
	{
		Expects(hasSelection());

		if (node == node_start)
		{
			if (offset_start > offset)
				std::swap(offset_start, offset);
			auto result = Do<RemoveText>(claim(node), offset_start, offset).perform();
			offset = offset_start;
			return result;
		}
		return {};
	}

	uptr<Reaction> eraseNext()
	{
		target_x = no_target;

		if (hasSelection())
			return eraseSelection();

		if (offset < maxOffset())
		{
			return Do<RemoveText>(claim(node), offset, utf8len(node->text[offset])).perform();
		}
		uptr<Reaction> result;
		if (node->space_after.empty())
		{
			return result;
			//if (!node->group.next())
			//	return {};
			//Expects(!text(*node->group.next()));
			//node->group.next()->removeFromGroup();
		}
		else
		{
			result = Do<RemoveSpace>(claim(node)).perform();
		}

		if (auto tnext = tex::as<tex::Text>(node->group.next()))
		{
			node->text.append(move(tnext->text));
			node->space_after = move(tnext->space_after);
			tnext->removeFromGroup();
			node->change();
		}
		return result;
	}
	uptr<Reaction> erasePrev()
	{
		target_x = no_target;

		if (hasSelection())
			return eraseSelection();

		if (offset > 0)
		{
			prev();
			return eraseNext();
		}
		uptr<Reaction> result;
		if (auto prev = node->group.prev())
		{
			if (prev->space_after.empty())
			{
				return result;
				//Expects(!text(*node->group.prev()));
				//node->group.prev()->removeFromGroup();
			}
			else
			{
				result = Do<RemoveSpace>(claim(prev)).perform();
			}

			if (auto pt = tex::as<tex::Text>(prev))
			{
				offset = pt->text.size();
				pt->text.append(move(node->text));
				pt->space_after = move(node->space_after);
				node->removeFromGroup();
				node = pt;
				node->change();
			}
		}
		return result;
		//else if (auto par = tex::as<tex::Par>(node->group()))
		//{
		//	if (auto prev_par = tex::as<tex::Par>(par->group.prev()))
		//	{
		//		while (!par->empty())
		//			prev_par->append(par->front().detachFromGroup());
		//		prev_par->space_after = move(par->space_after);
		//		par->removeFromGroup();
		//		prev_par->change();
		//		if (auto prevt = tex::as<tex::Text>(node->group.prev());
		//			prevt && prevt->space_after.empty())
		//		{
		//			offset = prevt->text.size();
		//			prevt->text.append(move(node->text));
		//			prevt->space_after = move(node->space_after);
		//			std::exchange(node, prevt)->removeFromGroup();
		//		}
		//	}
		//}
	}

	void insertSpace()
	{
		if (hasSelection())
			eraseSelection();

		if (offset == 0 && (!node->group.prev() || !node->group.prev()->space_after.empty()))
			return;
		node->insertSpace(offset);
		node = node->nextText();
		offset = 0;
		resetStart();
	}

	void breakParagraph()
	{
		if (hasSelection())
			eraseSelection();

		Expects(node);
		if (typeid(*node->group()) != typeid(tex::Par))
			return;
		if (offset == 0 && !node->group->contains(node->prevText()))
			return;

		node->group()->change();
		auto new_par = node->group()->insertAfterThis(tex::Group::make("par"));
		auto old_node = node;

		if (offset <= 0)
		{
			old_node = tex::as<tex::Text>(node->group.prev());
			if (!old_node)
				old_node = node->insertBeforeThis(tex::Text::make());
			new_par->append(node->detachFromGroup());
		}
		else if (offset >= node->text.size())
			node = new_par->append(tex::Text::make());
		else
		{
			node = new_par->append(tex::Text::make(node->text.substr(offset)));
			node->space_after = move(old_node->space_after);
			old_node->text.resize(offset);
		}
		while (old_node->group.next())
			new_par->append(old_node->group.next()->detachFromGroup());

		offset = 0;
		resetStart();
	}

	void nextStop()
	{
		if (auto new_node = node->nextStop())
		{
			node = new_node;
			offset = 0;
		}
		resetStart();
	}
	void prevStop()
	{
		if (auto new_node = node->prevStop())
		{
			node = new_node;
			offset = node->text.size();
		}
		resetStart();
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

class History
{
	Stack<uptr<Reaction>> _undo;
	Stack<uptr<Reaction>> _redo;
public:

	void add(uptr<Reaction> a) { _undo.push(move(a)); _redo.clear(); }

	void undo() { if (!_undo.empty()) _redo.push(_undo.pop()->perform()); }
	void redo() { if (!_redo.empty()) _undo.push(_redo.pop()->perform()); }
};

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
			case Key::home:  caret.home(); break;
			case Key::end:   caret.end();  break;
			case Key::right: caret.next(); break;
			case Key::left:  caret.prev(); break;
			case Key::up:     caret.up(context); break;
			case Key::down: caret.down(context); break;
			case Key::backspace: history.add(caret.erasePrev()); break;
			case Key::del:       history.add(caret.eraseNext()); break;
			case Key::space:     caret.insertSpace(); break;
			case Key::enter:     caret.breakParagraph(); break;
			case Key::tab:
				oui::pressed(Key::shift) ? 
					caret.prevStop() : 
					caret.nextStop();
				break;
			case Key::alt: case Key::f10:
				if (!is_repeat) 
					toggle_menu();
				break;
			case Key::z:
				if (oui::pressed(Key::ctrl))
				{
					oui::pressed(Key::shift) ?
						history.redo() :
						history.undo();
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

			history.add(Do<InsertText>(claim(caret.node), caret.offset, text).perform());

			caret.offset += text.length();
			caret.target_x = Caret::no_target;
			caret.resetStart();
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
				->insertAfterThis(move(result))
				->append(tex::Text::make());
			return;
		}
		if (caret.offset > 0)
		{
			auto prev_node = caret.node;
			caret.node = caret.node->insertAfterThis(tex::Text::make(caret.node->text.substr(caret.offset)));
			prev_node->text.resize(caret.offset);
			caret.offset = 0;
		}
		caret.node = caret.node
			->insertBeforeThis(move(result))
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
		for (const tex::Node* n = caret.node; ; n = n->group())
			if (auto par = tex::as<tex::Par>(n->group()))
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
			tokens->enforceRules();
			auto set_width = context.width.push(window.area().width());
			auto& tbox = tokens->updateLayout(context);
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
			auto optfont = context.fontData(tex::FontType::sans);

			const float h = 24;

			oui::set(oui::colors::black);
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
