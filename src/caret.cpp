#include "edit.h"

using std::move;

using oui::utf8len;

using namespace tex;

using namespace edit;

#pragma warning(disable: 26446)

void Caret::render(tex::Context & con)
{
	if (!con.window().focus())
		return;

	Expects(node != nullptr);
	Expects(node_start != nullptr);

	const auto render_one = [&con](const tex::Text& n, int o)
	{
		auto box = n.absBox();
		box.min.x += offsetXof(con, n, o) - 1;
		box.max.x = box.min.x + 2;
		oui::fill(box);
	};


	oui::set(oui::Blend::multiply);
	oui::set(oui::colors::black);
	render_one(*node, offset);


	oui::set(oui::Color{ 0.9,1,0.5 });

	const auto xe = offsetX(con);
	const auto xs = offsetXof(con, *node_start, offset_start);

	if (node == node_start)
	{
		const auto box = node->absBox();

		oui::fill({ { box.min.x + xs, box.min.y },{ box.min.x + xe, box.max.y } });

		return;
	}

	auto to_mark = interval(*node_start, *node);
	Expects(to_mark.size() >= 2);
	const auto fwd = to_mark.front() == node_start;

	{
		auto box = node->absBox();
		(fwd ? box.max.x : box.min.x) = box.min.x + xe;
		oui::fill(box);
		box = node_start->absBox();
		(fwd ? box.min.x : box.max.x) = box.min.x + xs;
		oui::fill(box);
	}

	for (auto it = ++to_mark.begin(), end = --to_mark.end(); it != end; ++it)
		oui::fill((*it)->absBox());
}

int Caret::repairOffset(int off) noexcept
{
	Expects(node && off < node->text.size());
	while (off > 0 && utf8len(node->text[off]) == 0)
		--off;
	return off;
}


uptr<Action> delete_if_redundant(const Text& node)
{
	if (!node.text.empty() ||
		!text(node.group.prev()) ||
		!text(node.group.next()))
		return {};

	return Do<RemoveNode>(claim_mutable(&node)).perform().undo;
}

uptr<Action> Caret::next()
{
	if (!node)
		return {};
	prepare(Move::forward);
	target_x = no_target;

	if (offset < node->text.size())
	{
		offset += utf8len(node->text[offset]);
		return {};
	}
	if (auto next_text = node->nextText())
	{
		offset = 0;
		return delete_if_redundant(*std::exchange(node, next_text));
	}
	return {};
}

uptr<Action> Caret::prev()
{
	if (!node)
		return {};
	prepare(Move::backward);
	target_x = no_target;

	if (offset > 0)
	{
		offset = repairOffset(offset - 1);
		return {};
	}
	if (auto prev_text = node->prevText())
	{
		offset = prev_text->text.size();
		return delete_if_redundant(*std::exchange(node, prev_text));
	}
	return {};
}

void Caret::findPlace(tex::Context & con)
{
	const auto font = con.fontData(node->font);
	const auto textdata = std::string_view(node->text);
	auto prev_x = node->absLeft();

	for (int i = 0, len = 1; i < int_size(textdata); i += len)
	{
		len = utf8len(node->text[i]);
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
			node = &e;
		}
	}
	return findPlace(con);
}

uptr<Action> Caret::up(tex::Context & con)
{
	if (!node)
		return {};
	prepare(Move::backward);

	const auto old_node = node;

	if (isnan(target_x))
		target_x = node->absLeft() + offsetX(con);
	if (!node->line())
		return {};

	const auto nextl = node->line->next();
	if (!nextl)
		return {};

	findClosestOnLine(con, nextl);
	return delete_if_redundant(*old_node);
}

uptr<Action> Caret::down(tex::Context & con)
{
	if (!node)
		return {};
	prepare(Move::forward);

	const auto old_node = node;

	if (isnan(target_x))
		target_x = node->absLeft() + offsetX(con);
	if (!node->line())
		return {};

	const auto prevl = node->line->prev();
	if (!prevl)
		return {};

	findClosestOnLine(con, prevl);
	return delete_if_redundant(*old_node);
}

uptr<Action> Caret::home()
{
	prepare(Move::backward);
	target_x = no_target;

	if (!node->line())
		return {};

	const auto old_node = node;
	node = &*node->line->begin();
	offset = 0;

	return node == old_node ? nullptr : 
		delete_if_redundant(*old_node);
}

uptr<Action> Caret::end()
{
	prepare(Move::forward);
	target_x = no_target;

	if (!node->line())
		return {};

	const auto old_node = node;
	node = &*node->line->rbegin();
	offset = node->text.size();

	return node == old_node ? nullptr :
		delete_if_redundant(*old_node);
}

uptr<Action> Caret::eraseSelection()
{
	Expects(hasSelection());

	if (node == node_start)
	{
		if (offset_start > offset)
			std::swap(offset_start, offset);
		return perform<RemoveText>(claim_mutable(node), offset_start, offset, Move::forward);
	}
	return {};
}

uptr<Action> Caret::eraseNext()
{
	target_x = no_target;

	if (hasSelection())
		return eraseSelection();

	if (offset < maxOffset())
	{
		return perform<RemoveText>(claim_mutable(node), offset, utf8len(node->text[offset]), Move::backward);
	}
	uptr<Action> result;
	if (node->space_after.empty())
	{
		return result;
		//if (!node->group.next())
		//	return {};
		//Expects(!text(*node->group.next()));
		//node->group.next()->removeFromGroup();
	}
	else if (auto nextt = as<Text>(node->group.next()))
	{
		return perform<MergeText>(claim_mutable(node), claim_mutable(nextt), Move::backward);
	}
	return result;
}

uptr<Action> Caret::erasePrev()
{
	target_x = no_target;

	if (hasSelection())
		return eraseSelection();

	if (offset > 0)
	{
		offset = repairOffset(offset-1);
		return perform<RemoveText>(claim_mutable(node), offset, utf8len(node->text[offset]), Move::forward);
	}
	uptr<Action> result;
	if (auto prev = node->group.prev())
	{
		if (prev->space_after.empty())
		{
			return result;
			//Expects(!text(*node->group.prev()));
			//node->group.prev()->removeFromGroup();
		}
		else if (auto prevt = as<Text>(prev))
		{
			return perform<MergeText>(claim_mutable(prevt), claim_mutable(node), Move::forward);
		}
		else
			return result;
	}
	auto par = as<Par>(node->group());
	if (!par) return {};
	auto prev_par = as<Par>(par->group.prev());
	if (!prev_par) return {};
	auto prev_end = as<Text>(&prev_par->back());
	Expects(prev_end != nullptr);

	return perform<UnsplitPar>(claim_mutable(prev_end), claim_mutable(par));
}

uptr<Action> Caret::insertSpace()
{
	if (hasSelection())
		return {}; // eraseSelection();
	if (offset <= 0) 
		return {};
	if (offset >= node->text.size())
		return perform<InsertNode>(Text::make("", " "), claim_mutable(node));

	return perform<SplitText>(claim_mutable(node), offset, " ", Move::forward);
}

[[nodiscard]] uptr<Action> Caret::breakParagraph()
{
	if (hasSelection())
		return {}; // eraseSelection();
	
	if (typeid(*node->group()) != typeid(tex::Par))
		return {};
	if (offset == 0 && !node->group->contains(node->prevText()))
		return {};
	
	return perform<SplitPar>(claim_mutable(node), offset);
}

void Caret::nextStop() noexcept
{
	if (auto new_node = node->nextStop())
	{
		node = new_node;
		offset = 0;
	}
	resetStart();
}

void Caret::prevStop() noexcept
{
	if (auto new_node = node->prevStop())
	{
		node = new_node;
		offset = node->text.size();
	}
	resetStart();
}
