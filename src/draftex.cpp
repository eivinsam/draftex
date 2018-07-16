#include <vector>
#include <string>
#include <string_view>
#include <variant>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include "tex_node.h"

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


struct Renderer : public tex::Node::Visitor
{
	tex::Context& con;

	oui::Vector offset;

	Renderer(tex::Context& con, oui::Vector offset = { 0,0 }) noexcept : con(con), offset(offset) { }

	void operator()(tex::Space&) noexcept override { }
	void operator()(tex::Group& group) override
	{
		offset = offset + group.box.offset;
		if (group.data == "math")
		{
			oui::fill(group.absBox(), oui::Color{ 0.1f, 0.2f, 1.0f, 0.1f });
		}
		if (group.data == "frac")
		{
			oui::fill(oui::align::centerLeft(oui::origo + offset).size({ group.box.width(), 1 }), oui::colors::black);
		}
		for (auto&& e : group)
			e.visit(Renderer{ con, offset });
	}
	void operator()(tex::Command& cmd) override 
	{
		con.font(tex::FontType::sans)->drawLine(offset + cmd.box.min(), cmd.data, 
			oui::Color{ .3f, .9f, .1f }, con.ptsize(tex::FontSize::normalsize));
	}
	void operator()(tex::Text& text) override 
	{
		con.font(text.font.type)->drawLine(offset + text.box.min(), text.data, 
			oui::colors::black, con.ptsize(text.font.size));
	}
};

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
		if (node->data.empty() && node->prev && node->next &&
			node->prev->isSpace() && node->next->isSpace())
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
				if (target_x >= above.absLeft())
				{
					node = (target_x - above.absRight() < prev_above->absLeft() - target_x) ? 
						&above : prev_above;
					return findPlace(con);
				}
				prev_above = &above;
			}
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
				if (target_x <= below.absRight())
				{
					node = (target_x - below.absLeft() > prev_below->absRight() - target_x) ?
						&below : prev_below;
					return findPlace(con);
				}
				prev_below = &below;
			}
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
				Expects(!node->next->isText());
				node->next->remove();
				if (node->next && node->next->isText())
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
				Expects(!node->prev->isText());
				node->prev->remove();
				if (node->prev && node->prev->isText())
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
		if (offset == 0 && (!node->prev || node->prev->isSpace()))
			return;
		const auto space = node->insertSpace(offset);
		node = space->nextText();
		offset = 0;
	}
};

int main()
{
	oui::Window window{ { "draftex", 1280, 720, 8 } };

	auto tokens = tex::tokenize(readFile("test.tex"));
	tokens->expand();

	tex::Context context(window);

	Caret caret;
	for (auto&& e : *tokens)
		if (auto group = tex::as<tex::Group>(&e); group && group->data == "document")
		{
			for (auto&& de : tex::as<tex::Group>(group->front()))
				if (auto text = tex::as<tex::Text>(&de))
				{
					caret.node = text;
					break;
				}
			break;
		}


	window.resize = [&](auto&&) { tokens->change(); };

	oui::input.keydown = [&](auto key)
	{
		using oui::Key;
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
		default: 
			return;
		}
		window.redraw();
	};
	oui::input.character = [&](int charcode)
	{
		if (charcode < 0 || charcode > 0xff)
			return;
		const char ch = gsl::narrow_cast<char>(charcode);
		if (ch < ' ')
			return;
		if (ch == ' ')
			return;

		caret.offset += caret.node->insert(caret.offset, oui::utf8(charcode));
		caret.target_x = Caret::no_target;
		window.redraw();
		return;
	};

	while (window.update())
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
		}

		oui::fill(caret.node->absBox(), { 0.0f, 0.1f, 1, 0.2f });
		for (tex::Group* p = caret.node->parent; p != nullptr; p = p->parent)
			if (p->data == "par")
				oui::fill(p->absBox(), { 0, 0, 1, 0.1f });

		tokens->visit(Renderer{ context });

		caret.render(context);
	}


	return 0;
}
