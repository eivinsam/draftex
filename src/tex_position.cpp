#include <tex_position.h>
#include <oui_unicode.h>

using oui::utf8len;

namespace tex
{
	void Position::recede()
	{
		Expects(valid());
		if (not atNodeStart())
		{
			--offset;
			while (not atNodeStart() && utf8len(node->text()[offset]) == 0)
				--offset;
		}
		else if (auto prev = node->prevText())
		{
			node = prev;
			offset = prev->text().size();
		}
	}

	void Position::advance()
	{
		Expects(valid());
		if (not atNodeEnd())
		{
			offset = std::min(node->text().size(), 
							  offset + utf8len(node->text()[offset]));
		}
		else if (auto next = node->nextText())
		{
			node = next;
			offset = 0;
		}
	}

	float Position::xOffset(tex::Context & con) const
	{
		return con.fontData(node->font)->offset(subview(node->text(), 0, offset), con.ptsize(node->font));
	}

	string_view Position::character() const
	{
		return string_view(node->text()).substr(offset, characterLength());
	}

	int Position::characterLength() const
	{
		return utf8len(node->text()[offset]);
	}

	void tex::Position::foo()
	{
	}

}
