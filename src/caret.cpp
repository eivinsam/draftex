#include "edit.h"
#include "..\include\tex_position.h"

using std::move;

using oui::utf8len;

using namespace tex;

using namespace edit;

#pragma warning(disable: 26446)


void Caret::render(tex::Context & con)
{
	if (!con.window().focus())
		return;

	Expects(current.valid());
	Expects(start.valid());

	const auto render_one = [&con](const Position& p)
	{
		auto box = p.node->absBox();
		box.min.x += p.xOffset(con) - 1;
		box.max.x = box.min.x + 2;
		oui::fill(box);
	};


	oui::set(oui::Blend::multiply);
	oui::set(oui::colors::black);
	render_one(current);

	if (not hasSelection())
		return;

	oui::set(oui::Color{ 0.9,1,0.5 });

	const auto xe = current.xOffset(con);
	const auto xs = start.xOffset(con);

	if (start.node == current.node)
	{
		auto box = current.node->absBox();

		box.max.x = box.min.x + std::max(xs, xe);
		box.min.x = box.min.x + std::min(xs, xe);
		oui::fill(box);

		return;
	}
	const auto nodes = interval(*start.node, *current.node);
	Expects(nodes.size() >= 2);
	{
		auto box = nodes.front()->absBox();
		box.min.x = box.min.x + (nodes.front() == start.node ? xs : xe);
		oui::fill(box);
		box = nodes.back()->absBox();
		box.max.x = box.min.x + (nodes.back() == start.node ? xs : xe);
		oui::fill(box);
	}
	for (auto&& n : xpr::those.from(nodes.begin()+1).until(nodes.end()-1))
		oui::fill(n->absBox());
}

uptr<Action> delete_if_redundant(const Text& node)
{
	if (!node.text().empty() ||
		!text(node.group.prev()) ||
		!text(node.group.next()))
		return {};

	return Do<RemoveNode>(&node).perform().undo;
}

uptr<Action> Caret::next()
{
	Expects(current.valid());
	prepare(Move::forward);
	target_x = no_target;

	const auto old_node = current.node;
	current.advance();
	
	if (current.node == old_node)
		return {};
	else
		return delete_if_redundant(*old_node);
}

uptr<Action> Caret::prev()
{
	Expects(current.valid());
	prepare(Move::backward);
	target_x = no_target;

	const auto old_node = current.node;
	current.recede();

	if (current.node == old_node)
		return {};
	else
		return delete_if_redundant(*old_node);
}

void Caret::findPlace(tex::Context & con)
{
	const auto font = con.fontData(current.node->font);
	auto&& textdata = current.node->text();
	auto prev_x = current.node->absLeft();

	for (int i = 0, len = 1; i < int_size(textdata); i += len)
	{
		len = utf8len(current.node->text()[i]);
		const auto x = prev_x + font->offset(textdata.substr(i, len), con.ptsize(current.node->font));
		if (x >= target_x)
		{
			current.offset = x - target_x > target_x - prev_x ? i : i + len;
			return;
		}
		prev_x = x;
	}
	current.offset = int_size(textdata);
}

void Caret::findClosestOnLine(tex::Context & con, tex::Line * line)
{
	if (!line)
		return;

	float closest_d = std::numeric_limits<float>::infinity();
	for (auto&& e : *line)
	{
		const auto abs_box = e.absBox();
		const float d = std::min(std::abs(target_x - abs_box.min.x), std::abs(target_x - abs_box.max.x));
		if (closest_d > d)
		{
			closest_d = d;
			current.node = &e;
		}
	}
	return findPlace(con);
}

uptr<Action> Caret::up(tex::Context & con)
{
	Expects(current.valid());
	prepare(Move::backward);

	const auto old_node = current.node;

	if (isnan(target_x))
		target_x = current.node->absLeft() + current.xOffset(con);
	if (!current.node->line())
		return {};

	const auto nextl = current.node->line->next();
	if (!nextl)
		return {};

	findClosestOnLine(con, nextl);
	return delete_if_redundant(*old_node);
}

uptr<Action> Caret::down(tex::Context & con)
{
	Expects(current.valid());
	prepare(Move::forward);

	const auto old_node = current.node;

	if (isnan(target_x))
		target_x = current.node->absLeft() + current.xOffset(con);
	if (!current.node->line())
		return {};

	const auto prevl = current.node->line->prev();
	if (!prevl)
		return {};

	findClosestOnLine(con, prevl);
	return delete_if_redundant(*old_node);
}

uptr<Action> Caret::home()
{
	Expects(current.valid());
	prepare(Move::backward);
	target_x = no_target;

	if (!current.node->line())
		return {};

	const auto old_node = current.node;
	current = tex::start(&*current.node->line->begin());

	return current.node == old_node ? nullptr : 
		delete_if_redundant(*old_node);
}

uptr<Action> Caret::end()
{
	prepare(Move::forward);
	target_x = no_target;

	if (!current.node->line())
		return {};

	const auto old_node = current.node;
	current = tex::end(&*current.node->line->rbegin());

	return current.node == old_node ? nullptr :
		delete_if_redundant(*old_node);
}

uptr<Action> Caret::eraseSelection(Move move)
{
	Expects(hasSelection());

	if (current.node == start.node)
	{
		if (start.offset > current.offset)
			std::swap(start.offset, current.offset);
		return perform<RemoveText>(start, current.offset - start.offset, move);
	}
	if (start == current.prev())
	{
		return perform<MergeText>(start.node, current.node, Move::forward);
	}
	if (start == current.next())
	{
		return perform<MergeText>(current.node, start.node, Move::backward);
	}
	return perform<EraseRange>(start, current);
}

uptr<Action> Caret::eraseNext()
{
	target_x = no_target;

	if (not hasSelection())
		start = current.next();
	return eraseSelection(Move::backward);
}

uptr<Action> Caret::erasePrev()
{
	target_x = no_target;

	if (not hasSelection())
		start = current.prev();
	return eraseSelection(Move::forward);
}

uptr<Action> Caret::insertSpace()
{
	if (hasSelection())
	{
		auto undo = make_action<Sequence>();
		undo->edits.push(eraseSelection(Move::forward));
		undo->edits.push(insertSpace());
		return undo;
	}
	if (current.atNodeStart()) 
		return {};
	if (current.atNodeEnd())
		return perform<InsertNode>(Text::make(" "), current.node);

	return perform<SplitText>(current, " ", Move::forward);
}

uptr<Action> Caret::insertText(string text)
{
	if (hasSelection())
	{
		auto undo = make_action<Sequence>();
		undo->edits.push(eraseSelection(Move::forward));
		undo->edits.push(insertText(move(text)));
		return undo;
	}
	return perform<InsertText>(current, move(text), Move::forward);
}

[[nodiscard]] uptr<Action> Caret::breakParagraph()
{
	if (hasSelection())
		return {}; // eraseSelection();
	
	if (typeid(*current.node->group()) != typeid(tex::Par))
		return {};
	if (current.atNodeStart() && !current.node->group->contains(current.node->prevText()))
		return {};
	
	return perform<SplitPar>(current);
}

void Caret::nextStop() noexcept
{
	if (auto new_node = current.node->nextStop())
		current = tex::start(new_node);
	resetStart();
}

void Caret::prevStop() noexcept
{
	if (auto new_node = current.node->prevStop())
		current = tex::end(new_node);
	resetStart();
}
