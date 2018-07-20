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
	class LineBulider
	{
		using iterator = Paragraph::iterator;
		int space_count;
		float max_above;
		float max_below;
		float width_left;
		oui::Vector pen;
		xpr::Range<iterator, iterator> rest;
		iterator it;

		enum class Align { left, justified };
	public:
		LineBulider(oui::Vector pen, iterator par_begin, iterator par_end) :
			pen(pen), rest(par_begin, par_end) { }

		bool done() const { return rest.empty(); }
		float height() const { return pen.y; }

		auto currentLine()
		{
			return xpr::those.from(rest.begin()).until(it);
		}

		void buildLine(float start_x, float width)
		{
			reset(start_x, width);

			skipSpaces();
			if (rest.empty())
				return;

			collectLine();

			position(it == rest.end() ? Align::left : Align::justified);

			rest.first = it;
		}
	private:
		void reset(float start_x, float width)
		{
			space_count = 0;
			max_above = 0;
			max_below = 0;
			pen.x = start_x;
			width_left = width;
			it = rest.first;
		}
		void skipSpaces()
		{
			while (it != rest.end() && space(*it))
				++it;
			rest.first = it;
		}
		void collectLine()
		{
			for (it = rest.begin(); it != rest.end(); ++it)
			{
				const auto box_width = it->box.width();
				if (width_left < box_width && it != rest.begin())
					break;
				space_count += space(*it) ? 1 : 0;
				max_above = std::max(max_above, it->box.above);
				max_below = std::max(max_below, it->box.below);
				width_left -= box_width;
			}
			unwindEndSpace();
		}
		void unwindEndSpace()
		{
			Expects(it != rest.begin());
			if (it != rest.end() && space(*it))
				return;

			--it;
			for (; space(*it); --it)
			{
				Expects(it != rest.begin());
				width_left += it->box.width();
				space_count -= 1;
			}
			++it;
		}
		void position(const Align alignment)
		{
			pen.y += max_above;
			for (auto&& e : currentLine())
			{
				e.updateLayout(pen);
				pen.x += e.box.width();

				if (alignment == Align::justified && space(e))
				{
					const float incr = width_left / space_count;
					width_left -= incr;
					space_count -= 1;
					pen.x += incr;
				}
			}
			pen.y += max_below;
		}
	};
	float Paragraph::updateLayout(oui::Vector pen, float width)
	{
		LineBulider builder(pen, begin(), end());

		while (!builder.done())
			builder.buildLine(pen.x, width);

		return builder.height();
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

	void Node::updateSize(Context& con, Mode, Font font, float /*width*/)
	{
		const auto F = con.font(font);
		box.width(F->offset(data, con.ptsize(font)), align::min);
		box.height(con.ptsize(font), align::center);
	}
	void Text::updateSize(Context& con, Mode new_mode, Font new_font, float /*width*/)
	{
		mode = new_mode;
		font = new_font;
		const auto F = con.font(font);
		box.width(F->offset(data, con.ptsize(font)), align::min);
		box.height(con.ptsize(font), align::center);
	}
	void Space::updateSize(Context& con, Mode mode, Font font, float /*width*/)
	{
		if (count(data, '\n') >= 2)
		{
			Expects(mode != Mode::math);
			box.width(0, align::min);
			box.height(0, align::min);
			return;
		}
		box.width(con.ptsize(font)*(mode == Mode::math ? 0 : 0.25f), align::min);
		box.height(con.ptsize(font), align::center);
	}

	class Frac : public Group
	{
	public:
		void updateSize(Context& con, Mode mode, Font font, float width) final
		{
			Expects(mode == Mode::math);
			const auto p = front().getArgument();
			const auto q = p->next->getArgument();

			font.size = shift(font.size, -2);

			p->updateSize(con, Mode::math, font, width);
			q->updateSize(con, Mode::math, font, width);

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

		void serialize(std::ostream& out) const final
		{
			out << "\\frac";
			for (auto&& e : *this)
				e.serialize(out);
		}
	};
	class VerticalGroup : public Group
	{
	public:
		void updateSize(Context& con, Mode mode, Font font, float width) final
		{
			if (data == "document")
			{
				font.type = FontType::roman;
				box.width(std::min(width, con.ptsize(font) * 24.0f), align::center);
				box.above = box.below = 0;
			}
			else
			{
				box.width(width, align::center);
				box.above = box.below = 0;
			}

			for (auto&& sub : *this)
			{
				sub.updateSize(con, mode, font, box.width());
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
		void serialize(std::ostream & out) const final
		{
			if (data == "document")
			{
				out << "\\begin{" << data << "}";
				_serialize_children(out);
				out << "\\end{" << data << "}";
				return;
			}
			_serialize_children(out);
		}
	};
	class Par : public Group
	{
		bool _needs_text_before(Node*) const final { return false; }
	public:
		void updateSize(Context& con, Mode mode, Font font, float width) final
		{
			box.width(width, align::min);
			box.height(0, align::min);

			for (auto&& sub : *this)
				sub.updateSize(con, mode, font, width);
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

	void Group::updateSize(Context& con, Mode mode, Font font, float width)
	{
		if (data == "math")
		{
			mode = Mode::math;
			font.type = FontType::italic;
		}
		box.before = box.after = 0;
		for (auto&& e : *this)
		{
			e.updateSize(con, mode, font, width);
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