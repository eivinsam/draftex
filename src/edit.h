#pragma once

#include "caret.h"

template <class T>
class Stack
{
	std::vector<T> _data;
public:

	bool empty() const noexcept { return _data.empty(); }

	void push(T&& value) { _data.push_back(std::move(value)); }
	void push(const T& value) { _data.push_back(value); }

	T& peek() { return _data.back(); }
	const T& peek() const { return _data.back(); }

	T pop() { auto result = move(_data.back()); _data.pop_back(); return result; }

	void clear() noexcept { _data.clear(); }
};

class Reaction
{
public:
	struct Result
	{
		uptr<Reaction> undo;
		Caret caret;
	};

	virtual ~Reaction() { }
	virtual Result perform() = 0;
};

uptr<Reaction> combine(const Reaction& first, const Reaction& second);

template <class T>
tex::Owner<T> claim(T* ptr) { return intrusive::refcount::claim(ptr); }


template <class A>
class Do : public Reaction, public A
{
public:
	template <class... Args>
	Do(Args&&... args) : A{ std::forward<Args>(args)... } { }

	[[nodiscard]] Result perform() final;
};

template <class A, class... Args>
uptr<Do<A>> make_action(Args&&... a)
{
	return std::make_unique<Do<A>>(std::forward<Args>(a)...);
}

template <class R, class... Args>
uptr<Reaction> Caret::perform(Args&&... args)
{
	auto[undo, caret] = Do<R>{ std::forward<Args>(args)... }.perform();
	*this = caret;
	return undo;
}

struct RemoveText
{
	tex::Owner<tex::Text> node;
	int offset;
	int length;
};
struct InsertText
{
	tex::Owner<tex::Text> node;
	int offset;
	tex::string text;
};

struct MergeText
{
	tex::Owner<tex::Text> first;
	tex::Owner<tex::Text> second;
	Caret::Move caret_move;
};
struct SplitText
{
	tex::Owner<tex::Text> node;
	int offset;
	tex::string space;
	Caret::Move caret_move;
};
struct UnmergeText
{
	tex::Owner<tex::Text> first;
	tex::Owner<tex::Text> second;
	Caret::Move caret_move;
};

