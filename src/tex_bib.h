#pragma once

#include <vector>
#include "tex.h"

namespace tex
{
	class Bib
	{
	public:
		struct Tag
		{
			string name;
			string content;
		};
		class Entry
		{
		public:
			string type;
			string name;

			std::vector<Tag> tags;

			const string* tag(string_view name) const;
		};

		std::vector<Entry> _entries;
	public:
		Bib(string_view in);

		const Entry* operator[](string_view name) const;
	};
}
