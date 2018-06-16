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

using gsl::narrow;

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

struct Renderer : public tex::Node::Visitor
{
	oui::Vector offset;
	tex::Context& con;

	Renderer(oui::Vector offset, tex::Context& con) noexcept : offset(offset), con(con) { }

	void operator()(tex::Space&) noexcept override { }
	void operator()(tex::Group& group) override
	{
		if (group.data == "math")
		{
			oui::fill(group.box + offset, oui::Color{ 0.1f, 0.2f, 1.0f, 0.1f });
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
		con.font(text.fonttype)->drawLine(text.box.min + offset, text.data, oui::colors::black);
	}
};

template <class Base, class Derived>
static constexpr bool is = 
	std::is_base_of_v<Base, std::remove_reference_t<std::remove_const_t<Derived>>>;

struct Caret
{
	static int length(gsl::not_null<const tex::Node*> node) 
	{ 
		return node->isText() ? narrow<int>(node->data.size()) : 1; 
	}

	static constexpr float no_target = std::numeric_limits<float>::quiet_NaN();

	tex::Node* node = nullptr;
	int offset = 0;
	float target_x = no_target;
	bool change = false;

	float offsetX(tex::Context& con) const
	{
		if (auto text = tex::as<tex::Text>(node))
			return node->box.min.x + con.font(text->fonttype)->offset(std::string_view(text->data).substr(0, offset));
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

		oui::fill(oui::align::topLeft(pos).size({ 2.0f, node->box.height() }), oui::colors::black);
	}

	int repairOffset(int off)
	{
		Expects(off < narrow<int>(node->data.size()));
		while (off > 0 && utf8len(node->data[off]) == 0)
			--off;
		return off;
	}

	void beforeMove()
	{
		if (node && node->prev() &&
			node->isSpace() && node->prev()->isSpace())
		{
			node->prev()->remove();
			change = true;
		}
	}

	void next()
	{
		if (!node)
			return;
		beforeMove();

		using namespace tex;
		struct V 
		{
			Caret& caret;

			void recurse(gsl::not_null<Node*> to)
			{
				caret.offset = -1;
				return to->visit(*this);
			}
			void escape(const Node& node)
			{
				if (node.next() && (caret.offset > 0 || !node.next()->isSpace()))
					return recurse(node.next());

				if (node.parent() && node.parent()->data != "root")
				{
					caret.offset = 1;
					return node.parent()->visit(*this);
				}

				caret.offset = length(&node);
				return;
			}
			void operator()(Node& node)
			{
				caret.node = &node;
				if (caret.offset < 0)
				{
					caret.offset = 0;
					return;
				}
				caret.offset = 1;
				return escape(node);
			}
			void operator()(Group& group)
			{
				if (caret.offset < 0)
				{
					if (!group.empty())
						return recurse(&group.front());
					caret.node = &group;
					caret.offset = 0;
					return;
				}
				caret.offset = 1;
				return escape(group);
			}
			void operator()(Space& space)
			{
				if (space.next())
					return recurse(space.next());
				else
				{
					caret.node = &space;
					caret.offset = 1;
				}
				return;
			}
			void operator()(Text& text)
			{
				caret.node = &text;
				if (caret.offset < 0)
				{
					caret.offset = 0;
					return;
				}
				if (caret.offset < narrow<int>(text.data.size()))
				{
					caret.offset += utf8len(gsl::at(text.data, caret.offset));

					if (caret.offset >= narrow<int>(text.data.size()) && text.next() && !text.next()->isSpace())
						return recurse(text.next());
					return;
				}
				return escape(text);
			}
		};

		target_x = no_target;
		return node->visit(V{ *this });
	}
	void prev()
	{
		if (!node)
			return;

		beforeMove();
		target_x = no_target;

		using namespace tex;
		struct V
		{
			Caret& caret;

			void recurse(gsl::not_null<Node*> to, int shift)
			{
				caret.offset = narrow<int>(to->data.size()) + shift;
				return to->visit(*this);
			}
			void escape(Node& node)
			{
				for (gsl::not_null<Node*> n = &node; ; n = n->parent())
				{
					if (!n->parent() || n->parent()->data == "root")
					{
						caret.offset = 0;
						return;
					}
					if (n->prev())
						return recurse(n->prev(), 1);
				}
			}
			void operator()(Node& node)
			{
				if (caret.offset <= 0)
					return escape(node);

				caret.node = &node;
				caret.offset = 0;
				return;
			}
			void operator()(Group& group)
			{
				if (caret.offset <= 0)
					return escape(group);

				if (!group.empty())
					return recurse(&group.back(), caret.offset-1);

				caret.node = &group;
				caret.offset = 0;
				return;
			}
			void operator()(Space& space)
			{
				return escape(space);
			}
			void operator()(Text& text)
			{
				caret.node = &text;
				if (const int textlen = narrow<int>(text.data.size()); caret.offset > textlen)
				{
					caret.offset = textlen;
					return;
				}
				if (caret.offset <= 0)
					return escape(text);
				caret.offset = caret.repairOffset(caret.offset-1);
				return;
			}
		};
		return node->visit(V{ *this });
	}

	void findPlace(tex::Context& con, const float target)
	{
		if (auto text = tex::as<tex::Text>(node))
		{
			const auto font = con.font(text->fonttype);
			const auto textdata = std::string_view(text->data);
			auto prev_x = node->box.min.x;

			for (int i = 0, len = 1; i < gsl::narrow<int>(textdata.size()); i += len)
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
			if (target - node->box.min.x > node->box.max.x - target)
			{
				if (node->next())
				{
					offset = 0;
					node = node->next();
				}
				else
				{
					offset = 1;
				}
				return;
			}
			while (node->isSpace() && node->prev())
			{
				node = node->prev();
				offset = length(node);
			}
		}
	}

	void up(tex::Context& con)
	{
		if (!node)
			return;
		beforeMove();

		if (isnan(target_x))
			target_x = offsetX(con);

		while (node->prev() && node->prev()->box.max.y > node->box.min.y)
			node = node->prev();

		while (node->prev())
		{
			node = node->prev();
			if (node->box.width() == 0 || node->box.height() == 0)
				continue;
			if (target_x > node->box.min.x)
			{
				findPlace(con, target_x);
				return;
			}
			if (node->prev()->box.max.y <= node->box.min.y)
			{
				offset = 0;
				return;
			}
		}
		offset = 0;
	}
	void down(tex::Context& con)
	{
		if (!node)
			return;
		beforeMove();

		if (isnan(target_x))
			target_x = offsetX(con);

		while (node->next() && node->next()->box.min.y < node->box.max.y)
			node = node->next();

		while (node->next())
		{
			node = node->next();
			if (node->box.width() == 0 || node->box.height() == 0)
				continue;
			if (target_x < node->box.max.x)
			{
				findPlace(con, target_x);
				return;
			}
			if (node->next()->box.min.y >= node->box.max.y)
			{
				offset = length(node);
				return;
			}
		}
		offset = length(node);
	}
	void home()
	{
		beforeMove();
		target_x = no_target;
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
		beforeMove();
		target_x = no_target;
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
				node->prev()->remove();
				if (node->prev() && node->isText() && node->prev()->isText())
				{
					node = node->prev();
					offset = narrow<int>(node->data.size());
					node->data.append(node->next()->data);
					node->next()->remove();
				}
				return;
			}
			if (node->prev())
			{
				prev();
				node->next()->remove();
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
		if (offset >= narrow<int>(node->data.size()))
		{
			if (!node->next())
				return;

			node = node->next();
			offset = 0;
			eraseNext();
			return;
		}
		node->change();
		node->data.erase(offset, utf8len(gsl::at(node->data, offset)));
		if (node->data.empty())
		{
			handle_empty();
		}
	}
	void erasePrev()
	{
		const auto* const old_node = node;
		const auto old_off = offset;
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


	window.resize = [&](auto&&) { tokens->change(); };

	oui::input.keydown = [&](auto key)
	{
		using oui::Key;
		switch (key)
		{
		case Key::right: caret.next(); break;
		case Key::left:  caret.prev(); break;
		case Key::up: caret.up(context); break;
		case Key::down: caret.down(context); break;
		case Key::home: caret.home(); break;
		case Key::end: caret.end(); break;
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
		caret.node->change();
		window.redraw();
		return;
	};

	while (window.update())
	{
		window.clear(oui::colors::white);
		context.reset(window);
		if (tokens->changed())
		{
			tokens->updateLayout(context, tex::FontType::sans, window.area().width());
			tokens->commit();
		}

		auto caret_box = caret.node->box;
		for (auto p = caret.node->parent(); p != nullptr; p = p->parent())
			caret_box = oui::align::topLeft(caret_box.min + (p->box.min - oui::origo)).size(caret_box.size());
		oui::fill(caret_box, { 0.0f, 0.1f, 1, 0.2f });

		tokens->visit(Renderer{ {0,0}, context });

		caret.render(context);
	}


	return 0;
}
