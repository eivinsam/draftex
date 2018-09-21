#include "edit.h"

#include <typeindex>

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

uptr<Reaction> Do<RemoveText>::perform()
{
	auto redo = make_action<InsertText>(node, offset, node->text.substr(offset, length));
	node->text.erase(offset, length);
	node->change();
	return redo;
}
uptr<Reaction> Do<InsertText>::perform()
{
	auto redo = make_action<RemoveText>(node, offset, text.size());
	node->text.insert(offset, text);
	node->change();
	return redo;
}

uptr<Reaction> Do<RemoveSpace>::perform()
{
	auto redo = make_action<InsertSpace>(node, node->space_after);
	node->space_after = "";
	node->change();
	return redo;
}
uptr<Reaction> Do<InsertSpace>::perform()
{
	auto redo = make_action<RemoveSpace>(node);
	node->space_after = space;
	node->change();
	return redo;
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

template <class F>
struct Dull;


template <auto f>
static std::pair<type_pair, Combiner> dull()
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
	dull<text_insert_combiner>(),
	dull<text_remove_combiner>()
};

uptr<Reaction> combine(const Reaction& first, const Reaction& second)
{
	const auto found = combiner_lookup.find(type_pair(typeid(first), typeid(second)));
	return found == combiner_lookup.end() ?
		nullptr :
		found->second(first, second);
}
