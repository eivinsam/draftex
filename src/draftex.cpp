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

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

std::string readFile(const std::string& filename)
{
	std::string result;
	std::ifstream file(filename, std::ios::binary);
	file.seekg(0, std::ios::end);
	if (!file)
		return result;
	auto size = static_cast<size_t>(file.tellg());
	result.resize(size);
	file.seekg(0, std::ios::beg);
	file.read(result.data(), result.size());
	return result;
}

struct Renderer : public tex::Node::Visitor
{
	oui::Vector offset;
	tex::Context& con;

	Renderer(oui::Vector offset, tex::Context& con) : offset(offset), con(con) { }

	void operator()(tex::Space&) override { }
	void operator()(tex::Group& group) override
	{
		if (group.data == "curly")
		{
			oui::fill(group.box + offset, oui::Color{ 0.8f, 0.7f, 0.0f, 0.3f });
		}
		const auto suboffset = offset + group.box.min - oui::Point{ 0,0 };
		for (auto&& e : group)
			e.visit(Renderer{ suboffset, con });
	}
	void operator()(tex::Command& cmd) override
	{
		con.font(tex::FontType::sans)->drawLine(cmd.box.min + offset, cmd.data, oui::Color{ .3f, .9f, .1f });
	}
	void operator()(tex::Comment& cmt) override
	{
		con.font(tex::FontType::sans)->drawLine(cmt.box.min + offset, cmt.data, oui::colors::blue);
	}
	void operator()(tex::Text& text) override 
	{
		con.font(text.font)->drawLine(text.box.min + offset, text.data, oui::colors::black);
	}
};


struct Caret
{
	static int length(tex::Node* node) { return node->isText() ? narrow<int>(node->data.size()) : 1; }

	tex::Node* node = nullptr;
	int offset = 0;

	float offsetX(tex::Context& con) const
	{
		if (auto text = tex::as<tex::Text>(node))
			return node->box.min.x + con.font(text->font)->offset(std::string_view(text->data).substr(0, offset));
		else
			return offset ? node->box.max.x : node->box.min.x;
	}

	void render(tex::Context& con)
	{
		if (!con.window().focus())
			return;

		oui::Point pos = { offsetX(con) - 1, node->box.min.y };

		for (auto p = node->parent(); p != nullptr; p = p->parent())
			pos = pos + (p->box.min - oui::origo);

		oui::fill(oui::topLeft(pos).size({ 2.0f, node->box.height() }), oui::colors::black);
	}

	int repairOffset(int off)
	{
		while (off > 0 && utf8len(node->data[off]) == 0)
			--off;
		return off;
	}

	bool next()
	{
		if (!node)
			return false;

		if (node->isSpace() && node->next() && node->prev() && node->prev()->isSpace())
		{
			node = node->next();
			node->prev()->detach();
			offset = 0;
			return true;
		}
		else if (node->isText())
		{
			const int len = narrow<int>(node->data.size());
			if (offset < len)
			{
				offset += utf8len(node->data[offset]);
				if (offset < len)
					return false;
			}
		}
		else
			offset += 1;

		while (node->next())
		{
			node = node->next();
			if (node->box.width() > 0)
			{
				offset = 0;
				return false;
			}
		}
		offset = length(node);
		return false;
	}
	bool prev()
	{
		if (!node)
			return false;

		if (offset > 0)
		{
			--offset;
			if (node->isText())
				offset = repairOffset(offset);
			return false;
		}

		if (node->prev())
		{
			if (node->isSpace() && node->prev()->isSpace())
			{
				node = node->prev();
				node->next()->detach();
				offset = 0;
				return true;
			}
		}

		while (node->prev())
		{
			const float old_x = node->box.min.x;
			node = node->prev();
			if (node->box.width() > 0)
			{
				offset = length(node);
				if (node->box.max.x == old_x)
					prev();
				return false;
			}
		}

		offset = 0;
		return false;
	}

	void findPlace(tex::Context& con, const float target)
	{
		if (auto text = tex::as<tex::Text>(node))
		{
			const auto font = con.font(text->font);
			const auto textdata = std::string_view(text->data);
			auto prev_x = node->box.min.x;
			for (int i = 0, len = 1; size_t(i) < textdata.size(); i += len)
			{
				len = utf8len(node->data[i]);
				const auto x = prev_x + font->offset(textdata.substr(i, len));
				if (x >= target)
				{
					offset = x - target > target - prev_x ? i : i + len;
					return;
				}
				prev_x = x;
			}
			offset = textdata.size();
		}
		else
		{
			offset = 0;
			if (target - node->box.min.x > node->box.max.x - target)
				next();
		}
	}

	void up(tex::Context& con)
	{
		if (!node)
			return;

		const auto target = offsetX(con);

		for (auto n = node->prev(); n != nullptr; n = n->prev())
			if (n->box.min.x <= target && target < n->box.max.x)
			{
				node = n;
				findPlace(con, target);
				return;
			}
	}
	void down(tex::Context& con)
	{
		if (!node)
			return;

		const auto target = offsetX(con);

		for (auto n = node->next(); n != nullptr; n = n->next())
			if (n->box.min.x <= target && target < n->box.max.x)
			{
				node = n;
				findPlace(con, target);
				return;
			}
	}
	void home()
	{
		while (node->prev())
		{
			if (node->prev()->box.min.x > node->box.min.x)
			{
				offset = 0;
				return;
			}
			node = node->prev();
		}
	}
	void end()
	{
		while (node->next())
		{
			if (node->next()->box.max.x < node->box.max.x)
			{
				offset = length(node);
				return;
			}
			node = node->next();
		}
	}

	void eraseNext()
	{
		const auto handle_empty = [this]
		{
			if (node->next())
			{
				node = node->next();
				node->prev()->detach();
				if (node->prev() && node->isText() && node->prev()->isText())
				{
					node = node->prev();
					offset = narrow<int>(node->data.size());
					node->data.append(node->next()->data);
					node->next()->detach();
				}
				return;
			}
			if (node->prev())
			{
				prev();
				node->next()->detach();
				return;
			}
		};

		if (!node->isText())
		{
			if (offset > 0)
				return;
			handle_empty();
			return;
		}
		node->data.erase(offset, utf8len(node->data[offset]));
		if (node->data.empty())
		{
			handle_empty();
		}
	}
	void erasePrev()
	{
		auto old_node = node;
		auto old_off = offset;
		prev();
		if (old_off != offset || old_node != node)
			eraseNext();
	}

	void insertSpace()
	{
		if (auto new_space = node->insertSpace(offset))
		{
			if (new_space->next())
			{
				node = new_space->next();
				offset = 0;
			}
			else
			{
				node = new_space;
				offset = 1;
			}
		}
	}
};

int main()
{
	oui::Window window{ { "draftex", 1280, 720 } };

	auto tokens = tex::tokenize(readFile("test.tex"));
	tokens->expand();

	tex::Context context(window);

	Caret caret;
	for (auto&& e : *tokens)
		if (auto group = tex::as<tex::Group>(&e); group && group->data == "document")
		{
			for (auto&& de : *group)
				if (de.isText())
				{
					caret.node = &de;
					break;
				}
			break;
		}

	bool layout_dirty = true;
	window.resize = [&](auto&&) { layout_dirty = true; };

	oui::input.keydown = [&](auto key)
	{
		using oui::Key;
		switch (key)
		{
		case Key::right: layout_dirty |= caret.next(); break;
		case Key::left:  layout_dirty |= caret.prev(); break;
		case Key::up: caret.up(context); break;
		case Key::down: caret.down(context); break;
		case Key::home: caret.home(); break;
		case Key::end: caret.end(); break;
		case Key::backspace: caret.erasePrev(); layout_dirty = true; break;
		case Key::del:       caret.eraseNext(); layout_dirty = true; break;
		case Key::space:     caret.insertSpace(); layout_dirty = true; break;
		default: 
			return;
		}
		window.redraw();
	};
	oui::input.character = [&](auto ch)
	{
		if (ch < ' ')
			return;
		if (ch == ' ')
			return;

		std::string utf8;
		if (ch < 0x80)
			utf8.push_back(ch);
		else if (ch < 0x800)
		{
			utf8.push_back(0xc0 | (ch >> 6));
			utf8.push_back(0x80 | (ch & 0x3f));
		}
		else
		{
			utf8.push_back(0xe0 | ((ch >> 12) & 0x0f));
			utf8.push_back(0x80 | ((ch >>  6) & 0x3f));
			utf8.push_back(0x80 | ( ch        & 0x3f));
		}
		caret.node->insert(caret.offset, utf8);
		caret.offset += narrow<int>(utf8.size());
		layout_dirty = true;
		window.redraw();
		return;
	};

	while (window.update())
	{
		window.clear(oui::colors::white);
		context.reset(window);
		if (std::exchange(layout_dirty, false))
		{
			tokens->updateLayout(context, tex::FontType::sans, window.area().width());
		}

		auto caret_box = caret.node->box;
		for (auto p = caret.node->parent(); p != nullptr; p = p->parent())
			caret_box = oui::topLeft(caret_box.min + (p->box.min - oui::origo)).size(caret_box.size());
		oui::fill(caret_box, { 0.0f, 0.1f, 1, 0.2f });

		tokens->visit(Renderer{ {0,0}, context });

		caret.render(context);
	}


	return 0;
}
