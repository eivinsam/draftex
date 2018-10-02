#pragma once

#include "tex_position.h"


template <class T, class D = std::default_delete<T>>
using uptr = std::unique_ptr<T, D>;

namespace edit
{
	class Action;
}

struct Caret
{
	using Position = tex::Position;
	using Action = edit::Action;

	static constexpr float no_target = std::numeric_limits<float>::quiet_NaN();

	enum class Move : char { none, backward, forward };

	struct From
	{
		Position value;

		explicit constexpr From(Position p) : value(p) { }
	};

	Position current;
	Position start;

	float target_x = no_target;

	constexpr Caret() = default;
	constexpr Caret(Position p) : current(p), start(p) { }
	constexpr Caret(From start, Position end) : current(end), start(start.value) { }

	constexpr bool hasSelection() const { return current != start; }

	constexpr void resetStart()
	{
		start = current;
	}

	template <class R, class... Args>
	uptr<Action> perform(Args&&... args);

	void render(tex::Context& con);


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

	[[nodiscard]] uptr<Action> eraseSelection(Move);

	[[nodiscard]] uptr<Action> eraseNext();
	[[nodiscard]] uptr<Action> erasePrev();

	[[nodiscard]] uptr<Action> insertSpace();
	[[nodiscard]] uptr<Action> insertText(tex::string);

	[[nodiscard]] uptr<Action> breakParagraph();

	void nextStop() noexcept;
	void prevStop() noexcept;
};

inline tex::Position operator+(tex::Position pos, Caret::Move move)
{
	using Move = Caret::Move;
	switch (move)
	{
	case Move::none: break;
	case Move::backward: pos.recede();  break;
	case Move::forward:  pos.advance(); break;
	default: Expects(false);
	}
	return pos;
}
