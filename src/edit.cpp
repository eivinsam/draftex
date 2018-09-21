#include "edit.h"

using namespace tex;

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
