#include "tex_bib.h"

#define SPACE_CASES case ' ': case '\t': case '\r': case '\n'

namespace tex
{
	static constexpr inline void skip(string_view& in) noexcept
	{
		in.remove_prefix(1);
	}

	static constexpr inline char pop(string_view& in) noexcept
	{
		const char result = in.front();
		in.remove_prefix(1);
		return result;
	}

	static inline bool is_in_token(char ch) noexcept
	{
		return isalnum(ch) || ch == '_';
	}
	static void read_token(string& out, string_view& in)
	{
		while (!in.empty() && is_in_token(in.front()))
			out.push_back(pop(in));
	}


	static constexpr inline void throw_on_empty(const std::string_view& in)
	{
		if (in.empty())
			throw IllFormed("unexpected end of bibtex data");
	}

	template <class P>
	void accept_space_expect(string_view& in, P&& expected, string_view unexpected_message)
	{
		for (;; skip(in))
			if (in.empty())
				throw IllFormed("unexpected end of bibtex data");
			else if (expected(in.front()))
				return;
			else if (!isspace(in.front()))
				throw IllFormed(unexpected_message);
	};

	static void match_brace(string& out, string_view& in)
	{
		for ( ; !in.empty(); out.push_back(pop(in)))
			switch (in.front())
			{
			case '{':
				out.push_back(pop(in));
				match_brace(out, in);
				continue;
			case '}':
				return;
			default:
				continue;
			}
		throw IllFormed("unexpected end of bibtex data");
	}

	static void expect_content(Bib::Tag& tag, string_view& in)
	{
		throw_on_empty(in);
		switch (in.front())
		{
		case '{': 
			skip(in);
			match_brace(tag.content, in);
			return;
		case '"':
			throw std::logic_error("quoted tag content not implemented");
		SPACE_CASES:
			return;
		default:
			throw IllFormed("expected tag content");
		}
	}

	static bool expect_tag(Bib::Entry& e, string_view& in)
	{
		for (;; skip(in))
			if (in.empty())
				throw IllFormed("unexpected end of bibtex data");
			else if (!isspace(in.front()))
				break;
		if (isalpha(in.front()))
		{
			auto& tag = e.tags.emplace_back();
			read_token(tag.name, in);
			
			accept_space_expect(in, is_any_of<'='>, "expected = after tag name");
			skip(in);

			expect_content(tag, in);
			return true;
		}
		if (in.front() == '}')
			return false;
		throw IllFormed("expected tag name");
	}

	static bool expect_comma(Bib::Entry& e, string_view& in)
	{
		throw_on_empty(in);
		switch (in.front())
		{
		case '}':
			return false;
		case ',':
			skip(in);
			return expect_tag(e, in);
		SPACE_CASES:
			return true;
		default:
			throw IllFormed("unexpected character while looking for comma of end of entry");
		}
	}

	constexpr struct
	{
		constexpr bool operator()(const Bib::Entry& a, const Bib::Entry& b) noexcept { return a.name < b.name; }
		constexpr bool operator()(const Bib::Entry& a, string_view name) noexcept { return a.name < name; }
	} by_name;
		
	Bib::Bib(string_view in)
	{
		for (;;)
		{
			for (;; skip(in))
				if (in.empty())
				{
					std::sort(_entries.begin(), _entries.end(), by_name);
					return;
				}
				else if (in.front() == '@')
					break;
				else if (!isspace(in.front()))
					throw IllFormed("expected @ before bibtex entry");

			skip(in);
			if (in.empty() || !isalpha(in.front()))
				throw IllFormed("expected letter after @");

			auto& e = _entries.emplace_back();
			read_token(e.type, in);
		
			accept_space_expect(in, is_any_of<'{'>, "expected { after bibtex type");
			skip(in);

			accept_space_expect(in, isalpha, "expected letter at start of bibtex entry name");
			read_token(e.name, in);

			while (expect_comma(e, in))
				skip(in);
			skip(in);
		}
	}

	const Bib::Entry* Bib::operator[](string_view name) const
	{
		auto lb = std::lower_bound(_entries.begin(), _entries.end(), name, by_name);
		if (lb != _entries.end() && lb->name == name)
			return std::addressof(*lb);
		else
			return nullptr;
	}
	const string* Bib::Entry::tag(string_view tag_name) const noexcept
	{
		for (auto&& e : tags)
			if (e.name == tag_name)
				return &e.content;
		return nullptr;
	}
}
