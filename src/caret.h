#pragma once

#include "tex_node.h"

static constexpr auto subview(std::string_view text, size_t off, size_t count) { return text.substr(off, count); }

template <class T, class D = std::default_delete<T>>
using uptr = std::unique_ptr<T, D>;

namespace edit
{
	class Action;
}

struct Caret
{
	using Action = edit::Action;

	static constexpr float no_target = std::numeric_limits<float>::quiet_NaN();

	enum class Move : char { none, backward, forward };

	const tex::Text* node = nullptr;
	const tex::Text* node_start = nullptr;
	int offset = 0;
	int offset_start = 0;
	float target_x = no_target;

	constexpr Caret() = default;
	constexpr Caret(const tex::Text* node, int offset) : 
		node(node), node_start(node), 
		offset(offset), offset_start(offset) { }
	constexpr Caret(const tex::Text* node_end, int offset_end, const tex::Text* node_start, int offset_start) :
		node(node_end), node_start(node_start),
		offset(offset_end), offset_start(offset_start) { }

	constexpr bool hasSelection() const { return offset != offset_start || node != node_start; }

	constexpr void resetStart()
	{
		node_start = node;
		offset_start = offset;
	}

	template <class R, class... Args>
	uptr<Action> perform(Args&&... args);

	int maxOffset() const noexcept { Expects(node != nullptr); return node->text().size(); }


	static float offsetXof(tex::Context& con, const tex::Text& n, int o)
	{
		return con.fontData(n.font)->offset(subview(n.text(), 0, o), con.ptsize(n.font));
	}

	float offsetX(tex::Context& con) const
	{
		Expects(node != nullptr);
		return offsetXof(con, *node, offset);
	}

	void render(tex::Context& con);

	int repairOffset(int off) noexcept;

	void prepare(Move /*move*/) noexcept
	{
	}

	[[nodiscard]] uptr<Action> next();
	[[nodiscard]] uptr<Action> prev();

	void findPlace(tex::Context& con);

	void findClosestOnLine(tex::Context& con, tex::Line* line);

	[[nodiscard]] uptr<Action> up(tex::Context& con);
	[[nodiscard]] uptr<Action> down(tex::Context& con);
	[[nodiscard]] uptr<Action> home();
	[[nodiscard]] uptr<Action> end();

	[[nodiscard]] uptr<Action> eraseSelection();

	[[nodiscard]] uptr<Action> eraseNext();
	[[nodiscard]] uptr<Action> erasePrev();

	[[nodiscard]] uptr<Action> insertSpace();

	[[nodiscard]] uptr<Action> breakParagraph();

	void nextStop() noexcept;
	void prevStop() noexcept;
};
