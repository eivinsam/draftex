#include "edit.h"

#include <typeindex>

using std::move;

using namespace tex;


struct pair_hasher
{
	template <class A, class B>
	constexpr size_t operator()(const std::pair<A, B>& pair) const 
	{
		auto lhs = std::hash<A>{}(pair.first);
		lhs ^= std::hash<B>{}(pair.second) +0x9e3779b9 + (lhs << 6) + (lhs >> 2);
		return lhs;
	}
};

#pragma warning(disable: 4458)

namespace edit
{

	using Result = Action::Result;

	template <class Result, class Proc>
	Result prog1(Result&& result, Proc&& proc)
	{
		std::forward<Proc>(proc)();
		return std::forward<Result>(result);
	}

	Result Do<Sequence>::perform()
	{
		Expects(not edits.empty());
		auto undo = make_action<Sequence>();
		Result result;
		while (not edits.empty())
		{
			result = edits.pop()->perform();
			undo->edits.push(move(result.undo));
		}
		result.undo = move(undo);
		return result;
	}

	Result Do<RemoveText>::perform()
	{
		const auto node = as_mutable(pos.node);
		node->markChange();
		return
		{
			make_action<InsertText>(pos, node->text().extract(pos.offset, length), caret_move),
			pos
		};
	}
	Result Do<InsertText>::perform()
	{
		const auto node = as_mutable(pos.node);
		node->markChange();
		node->text().insert(pos.offset, text);
		return
		{
			make_action<RemoveText>(pos, text.size(), caret_move),
			pos + (caret_move == Caret::Move::forward ? text.size() : 0)
		};
	}


	Result Do<SplitText>::perform()
	{
		const auto node = as_mutable(pos.node);
		auto next = Text::make(node->text().extract(pos.offset));
		node->insertAfterThis(next);
		node->markChange();
		next->space() = std::move(node->space());
		node->space() = move(space);
		return
		{
			make_action<MergeText>(node, next.get(), caret_move),
			(caret_move == Caret::Move::forward ? start(next) : end(node))
		};
	}
	Result Do<MergeText>::perform()
	{
		const auto a = as_mutable(first);
		const auto b = as_mutable(second);
		a->markChange();
		b->markChange();
		a->text().append(b->text());
		swap(a->space(), b->space());
		
		return
		{
			make_action<UnmergeText>(first, as<Text>(b->detachFromGroup()), caret_move),
			Position{ first, first->text().size() - second->text().size() }
		};
	}
	Result Do<UnmergeText>::perform()
	{
		const auto first = as_mutable(this->first);
		first->markChange();
		second->markChange();
		first->text().erase(first->text().size() - second->text().size());
		swap(first->space(), second->space());
		first->insertAfterThis(second);
		return
		{
			make_action<MergeText>(first, second.get(), caret_move),
			(caret_move == Caret::Move::forward ? start(second) : end(first))
		};
	}


	Result Do<InsertNode>::perform()
	{
		if (prev_to_be)
			as_mutable(prev_to_be)->insertAfterThis(node);
		else
			as_mutable(parent_to_be)->front().insertBeforeThis(node);
		node->markChange();
		return
		{
			make_action<RemoveNode>(node.get()),
			end(node->nextTextInclusive())
		};
	}
	Result Do<RemoveNode>::perform()
	{
		auto node = as_mutable(this->node);
		node->markChange();
		const auto prev = node->group.prev();
		const auto parent = node->group();
		return
		{
			make_action<InsertNode>(extract(node->detachFromGroup()), prev, parent),
			end(prev ? prev->prevTextInclusive() : parent->prevText())
		};
	}

	Result Do<SplitPar>::perform()
	{
		const auto par = as<Par>(as_mutable(pos.node->group()));
		Expects(par != nullptr);
		if (!new_par)
		{
			new_par = intrusive::refcount::make<Par>("par");
			new_par->append(Text::make());
			new_par->terminator = "\n\n";
		}
		const auto node = as_mutable(pos.node);
		auto new_node = as<Text>(&new_par->front());
		Expects(new_node != nullptr);
		new_node->text() = node->text().extract(pos.offset);
		swap(new_node->space(), node->space());

		std::swap(new_par->terminator, par->terminator);
		while (auto next = node->group.next())
			new_par->append(next->detachFromGroup());
		par->insertAfterThis(new_par);

		node->markChange();
		new_node->markChange();
		return
		{
			make_action<UnsplitPar>(node, new_par.get()),
			start(new_node)
		};
	}
	Result Do<UnsplitPar>::perform()
	{
		auto first = as<Par>(first_end->group());
		auto second_start = as<Text>(&second->front());
		Expects(first != nullptr && second_start != nullptr);
		std::swap(first->terminator, second->terminator);
		swap(first_end->space(), second_start->space());
		const int offset = first_end->text().size();
		first_end->text().append(second_start->text().extract(0));
		while (second_start->group.next())
			first->append(second_start->group.next()->detachFromGroup());

		first_end->markChange();
		auto second = as<Par>(this->second->detachFromGroup());
		Expects(second != nullptr);
		return
		{
			make_action<SplitPar>(Position{first_end, offset}, second),
			Position{ first_end, offset }
		};
	}



	Result Do<ChangeParType>::perform()
	{
		if (const auto par = [this]
		{
			for (const Group* p = pos.node->group(); p != nullptr; p = p->group())
				if (const Par* par = as<Par>(p))
					return as_mutable(par);
				return (Par*)nullptr;
		}())
		{
			const auto old_type = par->partype();
			par->partype(new_type);
			return
			{
				make_action<ChangeParType>(pos, old_type),
				pos
			};
		}
		else
			return { nullptr, pos };
	}

	Result Do<EraseRange>::perform()
	{
		Expects(start.node != end.node);
		auto undo = make_action<InsertRange>(start, end);
		auto to_remove = interval(*start.node, *end.node);
		
		if (to_remove.front() == end.node)
			std::swap(start, end);

		undo->edits.push(Do<RemoveText>(tex::start(end.node), end.offset).perform().undo);

		for (auto&& n : xpr::those.from(to_remove.rbegin()+1).until(to_remove.rend()-1))
			undo->edits.push(Do<RemoveNode>(extract(move(n))).perform().undo);

		auto sr = Do<RemoveText>(start, start.node->text().size() - start.offset).perform();
		undo->edits.push(move(sr.undo));

		if (start.node->group() == end.node->group())
		{
			sr = Do<MergeText>(start.node, end.node).perform();
			undo->edits.push(move(sr.undo));
		}
		sr.undo = move(undo);
		return sr;
	}

	Result Do<InsertRange>::perform()
	{
		while (not edits.empty())
			edits.pop()->perform();
		return
		{
			make_action<EraseRange>(start, end),
		{ Caret::From(start), end }
		};
	}



	static uptr<Action> text_insert_combiner(const Do<InsertText>& a, const Do<InsertText>& b)
	{
		if (a.pos.node != b.pos.node || a.caret_move != b.caret_move)
			return {};
		if (a.caret_move == Caret::Move::forward && a.pos.offset + a.text.size() == b.pos.offset)
			return make_action<InsertText>(a.pos, a.text + b.text, Caret::Move::forward);
		if (a.caret_move == Caret::Move::backward && a.pos.offset == b.pos.offset)
			return make_action<InsertText>(a.pos, b.text + a.text, Caret::Move::backward);
		return {};
	}
	static uptr<Action> text_remove_combiner(const Do<RemoveText>& a, const Do<RemoveText>& b)
	{
		if (a.pos == b.pos + b.length)
			return make_action<RemoveText>(b.pos, b.length + a.length);
		return {};
	}
	static uptr<Action> node_insert_remove_combiner(const Do<InsertNode>& a, const Do<RemoveNode>& b)
	{
		if (a.node == b.node)
			return std::make_unique<Annihilation>();
		return {};
	}
	static uptr<Action> text_unmerge_insert_combiner(const Do<UnmergeText>& a, const Do<InsertText>& b)
	{
		if (a.second == b.pos.node && b.pos.offset == 0 && a.second->text().size() == 0)
		{
			a.second->text() = b.text;
			return make_action<InsertNode>(a.second, a.first);
		}
		return {};
	}


	using type_pair = std::pair<std::type_index, std::type_index>;
	using Combiner = uptr<Action>(*)(const Action&, const Action&);

	template <class F>
	struct dull_resolver;
	template <class A, class B>
	struct dull_resolver<uptr<Action>(*)(const A&, const B&)>
	{
		using First = A;
		using Second = B;
	};

	template <class F> using First = typename dull_resolver<F>::First;
	template <class F> using Second = typename dull_resolver<F>::Second;

	template <auto f>
	static constexpr std::pair<type_pair, Combiner> dull() noexcept
	{
		using F = decltype(f);
		using resolver = dull_resolver<F>;
		using A = typename resolver::First;
		using B = typename resolver::Second;

		return
		{
			{ typeid(A), typeid(B) },

			+[](const Action& first, const Action& second) -> uptr<Action>
			{
				return f(
					reinterpret_cast<const A&>(first),
					reinterpret_cast<const B&>(second));
			}
		};
	}




	static std::unordered_map<type_pair, Combiner, pair_hasher> combiner_lookup =
	{
		dull<&text_insert_combiner>(),
		dull<&text_remove_combiner>(),
		dull<&node_insert_remove_combiner>(),
		dull<&text_unmerge_insert_combiner>()
	};

	uptr<Action> combine(const Action& first, const Action& second)
	{
		const auto found = combiner_lookup.find(type_pair(typeid(first), typeid(second)));
		if (found == combiner_lookup.end())
			return nullptr;

		return found == combiner_lookup.end() ?
			nullptr :
			found->second(first, second);
	}
}
