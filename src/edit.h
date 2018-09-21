#pragma once

#include "tex_node.h"

template <class T>
class Stack
{
	std::vector<T> _data;
public:

	bool empty() const { return _data.empty(); }

	void push(T&& value) { _data.push_back(std::move(value)); }
	void push(const T& value) { _data.push_back(value); }

	T& peek() { return _data.back(); }
	const T& peek() const { return _data.back(); }

	T pop() { auto result = move(_data.back()); _data.pop_back(); return result; }

	void clear() { _data.clear(); }
};

template <class T, class D = std::default_delete<T>>
using uptr = std::unique_ptr<T, D>;

class Reaction
{
public:
	virtual ~Reaction() { }
	virtual uptr<Reaction> perform() = 0;
};

template <class T>
tex::Owner<T> claim(T* ptr) { return intrusive::refcount::claim(ptr); }


template <class A>
class Do : public Reaction, public A
{
public:
	template <class... Args>
	Do(Args&&... args) : A{ std::forward<Args>(args)... } { }

	uptr<Reaction> perform() final;
};

template <class A, class... Args>
uptr<Do<A>> make_action(Args&&... a)
{
	return std::make_unique<Do<A>>(std::forward<Args>(a)...);
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

struct RemoveSpace
{
	tex::Owner<tex::Node> node;
};
struct InsertSpace
{
	tex::Owner<tex::Node> node;
	tex::string space;
};
