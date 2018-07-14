#include <tex_node.h>

using std::move;
using std::string;
using std::string_view;
using std::make_unique;

inline string_view view(const string& s) noexcept { return string_view{ s }; }

template <class V>
struct Default
{
	V value;
};
static constexpr struct
{
	template <class V>
	using rem_cvref_t = std::remove_const_t<std::remove_reference_t<V>>;
	template <class V>
	constexpr Default<rem_cvref_t<V>> operator=(V&& value) const
	{
		return { std::forward<V>(value) };
	}
} default_value;
template <class C, class K, class V>
V find(C&& map, const K& key, const Default<V>& default_value)
{
	if (const auto found = map.find(key);
		found != map.end())
		return found->second;
	return default_value.value;
}

namespace tex
{
	namespace align = oui::align;

	class Paragraph
	{
		std::vector<Node*> _nodes;

	public:

		void clear() { _nodes.clear(); }
		void push_back(Node* node) { _nodes.push_back(node); }

		// returns the resulting height
		float updateLayout(oui::Vector pen, float width);


		class iterator
		{
			friend class Paragraph;
			decltype(_nodes)::iterator _it;

			iterator(decltype(_it) it) : _it(move(it)) { }
		public:
			iterator() = default;
			iterator & operator++() { ++_it; return *this; }
			iterator & operator--() { --_it; return *this; }

			Node* operator->() { return *_it; }
			Node& operator*() { return **_it; }

			bool operator==(const iterator& other) const { return _it == other._it; }
			bool operator!=(const iterator& other) const { return _it != other._it; }
		};
		iterator begin() { return { _nodes.begin() }; }
		iterator end() { return { _nodes.end() }; }
	};
	float Paragraph::updateLayout(oui::Vector pen, float width)
	{
		const auto x0 = pen.x;
		auto rest = xpr::those.from(begin()).until(end());

		while (!rest.empty())
		{
			// skip initial spaces
			if (rest.first->isSpace())
			{
				++rest.first;
				continue;
			}
			// find line ned and collect line stats
			int space_count = 0;
			float max_above = 0;
			float max_below = 0;
			float width_left = width;
			iterator line_end;

			for (line_end = rest.begin(); line_end != rest.end(); ++line_end)
			{
				const auto box_width = line_end->box.width();
				if (width_left < box_width && line_end != rest.begin())
					break;
				space_count += line_end->isSpace() ? 1 : 0;
				max_above = std::max(max_above, line_end->box.above);
				max_below = std::max(max_below, line_end->box.below);
				width_left -= box_width;
			}
			// position line elements
			pen.y += max_above;
			if (line_end == rest.end())
			{
				for (auto&& e : rest)
				{
					e.updateLayout(pen);
					pen.x += e.box.width();
				}
				pen.y += max_below;
				return pen.y;
			}
			if (line_end != rest.begin() && line_end->isSpace())
			{
				--line_end;
				while (line_end != rest.begin() && line_end->isSpace())
				{
					--line_end;
					--space_count;
				}
				++line_end;
			}
			for (auto&& e : xpr::those.from(rest.begin()).until(line_end))
			{
				e.updateLayout(pen);
				pen.x += e.box.width();
				if (e.isSpace())
				{
					const float incr = ceil(width_left / space_count);
					width_left -= incr;
					space_count -= 1;
					pen.x += incr;
				}
			}
			rest.first = line_end;
			pen.y += max_below;
			pen.x = x0;
		}
		return pen.y;
	}


	bool Node::collect(Paragraph& out)
	{
		out.push_back(this);
		return true;
	}
	bool Group::collect(Paragraph& out)
	{
		if (data == "document" || data == "par")
			return false;
		if (data == "frac" || data == "math")
		{
			out.push_back(this);
			return true;
		}
		for (auto&& e : *this)
			e.collect(out);
		return true;
	}
	bool Space::collect(Paragraph& out)
	{
		if (count(data, '\n') >= 2)
			return false;
		out.push_back(this);
		return true;
	}
	Space * Text::insertSpace(int offset)
	{
		if (offset >= narrow<int>(data.size()))
		{
			return insertAfter(Space::make());
		}
		const auto offset_size = narrow<size_t>(offset);
		insertAfter(Text::make(string_view(data).substr(offset_size)));
		data.resize(offset_size);
		return insertAfter(Space::make());

	}

	void Node::updateSize(Context& con, FontType fonttype, float /*width*/)
	{
		const auto font = con.font(fonttype);
		box.width(font->offset(data), align::min);
		box.height(font->height(), align::center);
	}
	void Text::updateSize(Context& con, FontType new_fonttype, float /*width*/)
	{
		fonttype = new_fonttype;
		const auto font = con.font(fonttype);
		box.width(font->offset(data), align::min);
		box.height(font->height(), align::center);
	}
	void Space::updateSize(Context& con, FontType fonttype, float /*width*/)
	{
		if (count(data, '\n') >= 2)
		{
			box.width(0, align::min);
			box.height(0, align::min);
			return;
		}
		const auto font = con.font(fonttype);
		box.width(font->height()*0.25f, align::min);
		box.height(font->height(), align::center);
	}


	class Frac : public Group
	{
	public:
		void updateSize(Context& con, FontType fonttype, float width) final
		{
			const auto p = front().getArgument();
			const auto q = p->next->getArgument();

			p->updateSize(con, fonttype, width);
			q->updateSize(con, fonttype, width);

			box.width(std::max(p->box.width(), q->box.width()), align::min);
			box.above = p->box.height();
			box.below = q->box.height();
		}

		void updateLayout(oui::Vector offset) final
		{
			box.offset = offset;

			Node*const p = front().getArgument();
			Node*const q = p->next->getArgument();

			p->updateLayout({ (box.width() - p->box.width())*0.5f, -p->box.below });
			q->updateLayout({ (box.width() - q->box.width())*0.5f, +q->box.above });
		}
	};
	class VerticalGroup : public Group
	{
	public:
		void updateSize(Context& con, FontType fonttype, float width) final
		{
			if (data == "document")
			{
				fonttype = FontType::roman;
				box.width(std::min(width, con.font(fonttype)->height() * 24.0f), align::center);
				box.above = box.below = 0;
			}
			else
			{
				box.width(width, align::center);
				box.above = box.below = 0;
			}

			for (auto&& sub : *this)
			{
				sub.updateSize(con, fonttype, box.width());
				box.below += sub.box.height();
			}
		}
		void updateLayout(oui::Vector offset) final
		{
			box.offset = offset;

			float height = 0;
			for (auto&& sub : *this)
			{
				const auto sub_align = sub.box.before / sub.box.width();
				sub.updateLayout({ (sub_align - 0.5f)*box.width(), height + sub.box.above });
				height += sub.box.height();
			}
			box.above = 0;
			box.below = height;
		}
	};
	class Par : public Group
	{
	public:
		void updateSize(Context& con, FontType fonttype, float width) final
		{
			box.width(width, align::min);
			box.height(0, align::min);

			for (auto&& sub : *this)
				sub.updateSize(con, fonttype, width);
		}
		void updateLayout(oui::Vector offset) final
		{
			box.offset = offset;

			const float width = box.width();
			oui::Vector pen = { 0, 0 };

			Paragraph par;

			for (auto it = begin(); it != end(); ++it)
			{
				par.clear();
				if (it->collect(par))
				{
					++it;
					while (it != end() && it->collect(par))
						++it;
					pen.y = par.updateLayout(pen, width);
					if (it == end())  break;
				}
				it->updateLayout(pen);
				//it->box.tlc = l.place(align::topLeft(pen).size({ width, 0 }), it->box.size);
				pen.y += it->box.height();
			}
			box.height(pen.y, align::min);
		}
	};

	void Group::updateSize(Context& con, FontType fonttype, float width)
	{
		box.before = box.after = 0;
		for (auto&& e : *this)
		{
			e.updateSize(con, fonttype, width);
			box.above = std::max(box.above, e.box.above);
			box.below = std::max(box.below, e.box.below);
			box.after += e.box.width();
		}
	}

	void Node::updateLayout(oui::Vector offset)
	{
		box.offset = offset;
	}

	using LayoutUpdater = void(*)(Group&, oui::Vector);

	void Group::updateLayout(oui::Vector offset)
	{
		box.offset = offset;
		oui::Vector pen = { 0, 0 };
		for (auto&& e : *this)
		{
			e.updateLayout(pen);
			pen.x += e.box.width();
		}
	}

	template <class G>
	Owner<Group> make_group(string name)
	{
		auto result = make_unique<G>();
		result->data = move(name);
		return result;
	}
	Owner<Group> Group::make(string name)
	{
		static const std::unordered_map<string_view, Owner<Group>(*)(string)>
			maker_lookup =
		{
		{ "frac", make_group<Frac> },
		{ "par", make_group<Par> },
		{ "root", make_group<VerticalGroup> },
		{ "document", make_group<VerticalGroup> }
		};

		return find(maker_lookup, name, default_value = &make_group<Group>)(move(name));
	}
}