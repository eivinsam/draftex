#pragma once

#include "edit.h"

static constexpr auto subview(std::string_view text, size_t off, size_t count) { return text.substr(off, count); }

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

	void render(tex::Context& con);

	int repairOffset(int off);

	void check_for_deletion(tex::Text& n);

	void prepare(Move /*move*/)
	{
	}

	void next();
	void prev();

	void findPlace(tex::Context& con);

	void findClosestOnLine(tex::Context& con, tex::Line* line);

	void up(tex::Context& con);
	void down(tex::Context& con);
	void home();
	void end();

	uptr<Reaction> eraseSelection();

	uptr<Reaction> eraseNext();
	uptr<Reaction> erasePrev();

	void insertSpace();

	void breakParagraph();

	void nextStop();
	void prevStop();
};
