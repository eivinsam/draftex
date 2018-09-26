#include "edit.h"

#include <typeindex>

using std::move;

using namespace tex;

Caret start(Text* node) noexcept { return { node, 0 }; }
Caret end(Text* node) noexcept { return { node, node ? node->text.size() : 0 }; }

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

namespace edit
{

	using Result = Action::Result;

	template <class Result, class Proc>
	Result prog1(Result&& result, Proc&& proc)
	{
		std::forward<Proc>(proc)();
		return std::forward<Result>(result);
	}

	Result Do<RemoveText>::perform()
	{
		return
		{
			make_action<InsertText>(node, offset, node->extract(offset, length), caret_move),
		{ node.get(), offset }
		};
	}
	Result Do<InsertText>::perform()
	{
		return
		{
			make_action<RemoveText>(node, offset, node->insert(offset, text), caret_move),
		{ node.get(), offset + (caret_move == Caret::Move::forward ? text.size() : 0) }
		};
	}


	Result Do<SplitText>::perform()
	{
		Text* next = node->insertAfterThis(Text::make(node->extract(offset)));
		node->markChange();
		node->group.next()->space_after = std::exchange(node->space_after, move(space));
		return
		{
			make_action<MergeText>(node, claim(next), caret_move),
			(caret_move == Caret::Move::forward ? start(next) : end(node.get()))
		};
	}
	Result Do<MergeText>::perform()
	{
		first->markChange();
		second->markChange();
		first->text.append(second->text);
		std::swap(first->space_after, second->space_after);
		second->removeFromGroup();
		return
		{
			make_action<UnmergeText>(first, second, caret_move),
		{ first.get(), first->text.size() - second->text.size() }
		};
	}
	Result Do<UnmergeText>::perform()
	{
		first->markChange();
		second->markChange();
		first->text.resize(first->text.size() - second->text.size());
		std::swap(first->space_after, second->space_after);
		first->insertAfterThis(second);
		return
		{
			make_action<MergeText>(first, second, caret_move),
			(caret_move == Caret::Move::forward ? start(second.get()) : end(first.get()))
		};
	}


	Result Do<InsertNode>::perform()
	{
		prev_to_be->insertAfterThis(node);
		node->markChange();
		std::swap(prev_to_be->space_after, node->space_after);
		return
		{
			make_action<RemoveNode>(node),
			end(node->nextTextInclusive())
		};
	}
	Result Do<RemoveNode>::perform()
	{
		const auto prev = node->group.prev();
		Expects(prev != nullptr);
		std::swap(prev->space_after, node->space_after);
		node->markChange();
		node->removeFromGroup();
		return
		{
			make_action<InsertNode>(node, claim(prev)),
			end(prev->prevTextInclusive())
		};
	}


	static uptr<Action> text_insert_combiner(const Do<InsertText>& a, const Do<InsertText>& b)
	{
		if (a.node != b.node || a.caret_move != b.caret_move)
			return {};
		if (a.caret_move == Caret::Move::forward && a.offset + a.text.size() == b.offset)
			return make_action<InsertText>(a.node, a.offset, a.text + b.text, Caret::Move::forward);
		if (a.caret_move == Caret::Move::backward && a.offset == b.offset)
			return make_action<InsertText>(a.node, a.offset, b.text + a.text, Caret::Move::backward);
		return {};
	}
	static uptr<Action> text_remove_combiner(const Do<RemoveText>& a, const Do<RemoveText>& b)
	{
		if (a.node == b.node && b.offset + b.length == a.offset)
			return make_action<RemoveText>(b.node, b.offset, b.length + a.length);
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
		if (a.second == b.node && b.offset == 0 && a.second->text.size() == 0)
		{
			a.second->text = b.text;
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
