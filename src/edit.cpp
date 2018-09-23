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

using Result = Reaction::Result;

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
		make_action<InsertText>(node, offset, node->extract(offset, length)), 
	{ node.get(), offset } 
	};
}
Result Do<InsertText>::perform()
{
	return 
	{
		make_action<RemoveText>(node, offset, node->insert(offset, text)),
	{ node.get(), offset + text.size() }
	};
}


Result Do<SplitText>::perform()
{
	const auto fwd = caret_move == Caret::Move::forward;
	Text* next = node->insertAfterThis(Text::make(node->extract(offset)));
	node->change();
	node->group.next()->space_after = std::exchange(node->space_after, move(space));
	return 
	{
		make_action<MergeText>(node, claim(next), caret_move),
	{ fwd ? next : node.get(), fwd ? 0 : node->text.size() }
	};
}
Result Do<MergeText>::perform()
{
	first->change();
	second->change();
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
	const auto fwd = caret_move == Caret::Move::forward;
	first->change();
	second->change();
	first->text.resize(first->text.size() - second->text.size());
	std::swap(first->space_after, second->space_after);
	first->insertAfterThis(second);
	return
	{
		make_action<MergeText>(first, second, caret_move),
	{ fwd ? second.get() : first.get(), fwd ? 0 : first->text.size() }
	};
}

static uptr<Reaction> text_insert_combiner(const Do<InsertText>& a, const Do<InsertText>& b)
{
	if (a.node == b.node && a.offset + a.text.size() == b.offset)
		return make_action<InsertText>(a.node, a.offset, a.text + b.text);
	return {};
}
static uptr<Reaction> text_remove_combiner(const Do<RemoveText>& a, const Do<RemoveText>& b)
{
	if (a.node == b.node && b.offset + b.length == a.offset)
		return make_action<RemoveText>(b.node, b.offset, b.length + a.length);
	return {};
}


using type_pair = std::pair<std::type_index, std::type_index>;
using Combiner = uptr<Reaction>(*)(const Reaction&, const Reaction&);

template <class F>
struct dull_resolver;
template <class A, class B>
struct dull_resolver<uptr<Reaction>(*)(const A&, const B&)>
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

		+[](const Reaction& first, const Reaction& second) -> uptr<Reaction>
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
	dull<&text_remove_combiner>()
};

uptr<Reaction> combine(const Reaction& first, const Reaction& second)
{
	const auto found = combiner_lookup.find(type_pair(typeid(first), typeid(second)));
	return found == combiner_lookup.end() ?
		nullptr :
		found->second(first, second);
}
