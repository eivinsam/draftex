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

void render(const oui::Vector& offset, const tex::Node& node, tex::Context& con)
{
	using tex::Type;
	switch (node.type())
	{
	case Type::text:
	case Type::command:
	case Type::comment:
		con.font(node.font)->drawLine(node.box.min + offset, node.data, oui::colors::black);
		return;
	case Type::space:
		return;
	case Type::group:
	{
		if (node.data == "curly")
		{
			oui::fill(node.box + offset, oui::Color{ 0.8f, 0.7f, 0.0f, 0.3f });
		}
		const auto suboffset = offset + node.box.min - oui::Point{ 0,0 };
		for (auto&& e : node)
			render(suboffset, e, con);
		return;
	}
	default:
		throw std::logic_error("unknown tex::Type");
	}
}

int utf8len(unsigned char ch)
{
	switch (ch >> 4)
	{
	case 0xf: return 4;
	case 0xe: return 3;
	case 0xd: case 0xc: return 2;
	default:
		return (ch >> 7) ^ 1;
	}
}

struct Caret
{
	tex::Node* node = nullptr;
	int offset = 0;

	void render(tex::Context& con)
	{
		if (!con.window().focus())
			return;

		auto pos = node->box.min;
		pos.x += con.font(node->font)->offset(std::string_view(node->data).substr(0, offset)) -1;
		for (auto p = node->parent(); p != nullptr; p = p->parent())
			pos = pos + (p->box.min - oui::origo);

		oui::fill(oui::topLeft(pos).size({ 2.0f, float(con.font(node->font)->height()) }), oui::colors::black);
	}

	int repairOffset(int off)
	{
		while (off > 0 && utf8len(node->data[off]) == 0)
			--off;
		return off;
	}

	void next()
	{
		if (!node)
			return;

		if (offset < node->length())
		{
			offset += node->isText() ? utf8len(node->data[offset]) : 1;
			if (offset < node->length() || 
				!node->next() || node->next()->box.min.x != node->box.max.x)
				return;
		}
		while (node->next())
		{
			node = node->next();
			if (node->box.width() > 0)
			{
				offset = 0;
				return;
			}
		}
		offset = node->length();
	}
	void prev()
	{
		if (!node)
			return;
		if (offset > 0)
		{
			--offset;
			if (node->isText())
				offset = repairOffset(offset);
			return;
		}
		while (node->prev())
		{
			const float old_x = node->box.min.x;
			node = node->prev();
			if (node->box.width() > 0)
			{
				offset = node->length();
				if (node->box.max.x == old_x)
					prev();
				return;
			}
		}
		offset = 0;
	}

	void findPlace(tex::Context& con, const float target)
	{
		if (!node->isText())
		{
			offset = 0;
			if (target - node->box.min.x > node->box.max.x - target)
				next();
			return;
		}
		const auto font = con.font(node->font);
		const auto text = std::string_view(node->data);
		auto prev_x = node->box.min.x;
		for (int i = 0, len = 1; size_t(i) < text.size(); i += len)
		{
			len = utf8len(node->data[i]);
			const auto x = prev_x + font->offset(text.substr(i, len));
			if (x >= target)
			{
				offset = x - target > target - prev_x ? i : i + len;
				return;
			}
			prev_x = x;
		}
		offset = text.size();
	}

	void up(tex::Context& con)
	{
		if (!node)
			return;

		const auto target = node->box.min.x 
			+ con.font(node->font)->offset(std::string_view(node->data).substr(0, offset));

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

		const auto target = node->box.min.x
			+ con.font(node->font)->offset(std::string_view(node->data).substr(0, offset));

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
				offset = node->length();
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
				node->prev()->detatch();
				return;
			}
			if (node->prev())
			{
				prev();
				node->next()->detatch();
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
	bool insertSpace()
	{
		if (offset == 0)
		{
			if (node->prev()->isSpace())
				return false;
			node->insertBefore(std::make_unique<tex::Node>(" ")); 
			return true;
		}
		return false;
	}
	void insertText(std::string text)
	{
		if (offset == 0)
		{
			if (node->prev()->isText())
			{
				node->prev()->data.append(text);
				return;
			}
			node->insertBefore(std::make_unique<tex::Node>(std::move(text)));
			return;
		}
		return;
	}
};

int main()
{
	oui::Window window{ { "draftex", 1280, 720 } };

	auto tokens = tex::tokenize(readFile("test.tex"));

	tex::Context context(window);

	Caret caret;
	for (auto&& e : tokens)
		if (e.data == "document")
		{
			for (auto&& de : e)
				if (de.type() == tex::Type::text)
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
		case Key::right: caret.next(); break;
		case Key::left: caret.prev(); break;
		case Key::up: caret.up(context); break;
		case Key::down: caret.down(context); break;
		case Key::home: caret.home(); break;
		case Key::end: caret.end(); break;
		case Key::backspace: caret.erasePrev(); layout_dirty = true; break;
		case Key::del:       caret.eraseNext(); layout_dirty = true; break;
		case Key::space: if (caret.insertSpace()) layout_dirty = true; break;
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
		caret.insertText(utf8);
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
			tokens.updateLayout(context, tex::FontType::sans, window.area().width());
		}

		render({ 0,0 }, tokens, context);

		caret.render(context);
	}


	return 0;
}
