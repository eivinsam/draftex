#pragma once

#include "caret.h"

namespace edit
{
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

	class Action
	{
	public:
		struct Result
		{
			uptr<Action> undo;
			Caret caret;
		};

		virtual ~Action() { }
		virtual Result perform() = 0;
	};

	struct Annihilation : Action { Result perform() final { return {}; } };

	uptr<Action> combine(const Action& first, const Action& second);

	template <class A>
	class Do : public Action, public A
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

	struct RemoveText
	{
		tex::Owner<tex::Text> node;
		int offset;
		int length;
		Caret::Move caret_move;
	};
	struct InsertText
	{
		tex::Owner<tex::Text> node;
		int offset;
		tex::string text;
		Caret::Move caret_move;
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

	struct InsertNode
	{
		tex::Owner<tex::Node> node;
		tex::Owner<tex::Node> prev_to_be;
	};
	struct RemoveNode
	{
		tex::Owner<tex::Node> node;
	};

	struct SplitPar
	{
		tex::Owner<tex::Text> node;
		int offset;
		tex::Owner<tex::Par> new_par;
	};
	struct UnsplitPar
	{
		tex::Owner<tex::Text> first_end;
		tex::Owner<tex::Par> second;
	};

	struct ChangeParType
	{
		tex::Position pos;
		tex::Par::Type new_type;
	};
}
template <class R, class... Args>
uptr<edit::Action> Caret::perform(Args&&... args)
{
	auto[undo, caret] = edit::Do<R>{ std::forward<Args>(args)... }.perform();
	*this = caret;
	return undo;
}
