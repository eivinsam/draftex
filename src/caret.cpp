#include "edit.h"

using std::move;

using oui::utf8len;

using namespace tex;

#pragma warning(disable: 26446)

void Caret::render(tex::Context & con)
{
	if (!con.window().focus())
		return;

	Expects(node != nullptr);
	Expects(node_start != nullptr);

	const auto render_one = [&con](tex::Text& n, int o)
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

void Caret::check_for_deletion(tex::Text & n)
{
	const auto rem_node = [](tex::Node& n) noexcept
	{
		if (auto prev = n.group.prev();
			prev && prev->space_after.empty())
			prev->space_after = std::move(n.space_after);
		n.group->change();
		n.removeFromGroup();
	};

	if (&n == node)
		return;

	if (n.text.empty())
	{
		auto g = n.group();
		rem_node(n);
		while (g && g->empty())
			rem_node(*std::exchange(g, g->group()));
	}

}

void Caret::next()
{
	if (!node)
		return;
	prepare(Move::forward);
	target_x = no_target;

	if (offset < node->text.size())
	{
		offset += utf8len(node->text[offset]);
		return;
	}
	if (auto next_text = node->nextText())
	{
		check_for_deletion(*std::exchange(node, next_text));
		offset = 0;
		return;
	}
}

void Caret::prev()
{
	if (!node)
		return;
	prepare(Move::backward);
	target_x = no_target;

	if (offset > 0)
	{
		offset = repairOffset(offset - 1);
		return;
	}
	if (auto prev_text = node->prevText())
	{
		check_for_deletion(*std::exchange(node, prev_text));
		offset = node->text.size();
		return;
	}
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

	auto& original_node = *node;

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
	check_for_deletion(original_node);
	return findPlace(con);
}

void Caret::up(tex::Context & con)
{
	if (!node)
		return;
	prepare(Move::backward);

	if (isnan(target_x))
		target_x = node->absLeft() + offsetX(con);
	if (node->line())
		findClosestOnLine(con, node->line->next());
}

void Caret::down(tex::Context & con)
{
	if (!node)
		return;
	prepare(Move::forward);

	if (isnan(target_x))
		target_x = node->absLeft() + offsetX(con);
	if (node->line())
		findClosestOnLine(con, node->line->prev());
}

void Caret::home()
{
	prepare(Move::backward);
	target_x = no_target;

	if (node->line())
	{
		check_for_deletion(*std::exchange(node, &*node->line->begin()));
		offset = 0;
	}
}

void Caret::end()
{
	prepare(Move::forward);
	target_x = no_target;

	if (node->line())
	{
		check_for_deletion(*std::exchange(node, &*node->line->rbegin()));
		offset = node->text.size();
	}
}

uptr<Reaction> Caret::eraseSelection()
{
	Expects(hasSelection());

	if (node == node_start)
	{
		if (offset_start > offset)
			std::swap(offset_start, offset);
		return perform<RemoveText>(claim(node), offset_start, offset);
	}
	return {};
}

uptr<Reaction> Caret::eraseNext()
{
	target_x = no_target;

	if (hasSelection())
		return eraseSelection();

	if (offset < maxOffset())
	{
		return perform<RemoveText>(claim(node), offset, utf8len(node->text[offset]));
	}
	uptr<Reaction> result;
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
		return perform<MergeText>(claim(node), claim(nextt), Move::backward);
	}
	return result;
}

uptr<Reaction> Caret::erasePrev()
{
	target_x = no_target;

	if (hasSelection())
		return eraseSelection();

	if (offset > 0)
	{
		prev();
		resetStart();
		return eraseNext();
	}
	uptr<Reaction> result;
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
			return perform<MergeText>(claim(prevt), claim(node), Move::forward);
		}
	}
	return result;
	//else if (auto par = tex::as<tex::Par>(node->group()))
	//{
	//	if (auto prev_par = tex::as<tex::Par>(par->group.prev()))
	//	{
	//		while (!par->empty())
	//			prev_par->append(par->front().detachFromGroup());
	//		prev_par->space_after = move(par->space_after);
	//		par->removeFromGroup();
	//		prev_par->change();
	//		if (auto prevt = tex::as<tex::Text>(node->group.prev());
	//			prevt && prevt->space_after.empty())
	//		{
	//			offset = prevt->text.size();
	//			prevt->text.append(move(node->text));
	//			prevt->space_after = move(node->space_after);
	//			std::exchange(node, prevt)->removeFromGroup();
	//		}
	//	}
	//}
}

uptr<Reaction> Caret::insertSpace()
{
	if (hasSelection())
		return {}; // eraseSelection();

	if (offset <= 0 || offset >= node->text.size())
		return {};

	return perform<SplitText>(claim(node), offset, " ", Move::forward);
}

void Caret::breakParagraph()
{
	if (hasSelection())
		return; // eraseSelection();

	Expects(node != nullptr);
	if (typeid(*node->group()) != typeid(tex::Par))
		return;
	if (offset == 0 && !node->group->contains(node->prevText()))
		return;

	node->group()->change();
	const auto new_par = node->group()->insertAfterThis(tex::Group::make("par"));
	auto old_node = node;

	if (offset <= 0)
	{
		old_node = tex::as<tex::Text>(node->group.prev());
		if (!old_node)
			old_node = node->insertBeforeThis(tex::Text::make());
		new_par->append(node->detachFromGroup());
	}
	else if (offset >= node->text.size())
		node = new_par->append(tex::Text::make());
	else
	{
		node = new_par->append(tex::Text::make(node->text.substr(offset)));
		node->space_after = move(old_node->space_after);
		old_node->text.resize(offset);
	}
	while (old_node->group.next())
		new_par->append(old_node->group.next()->detachFromGroup());

	offset = 0;
	resetStart();
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
