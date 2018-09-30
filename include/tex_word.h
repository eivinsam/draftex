#pragma once

#include "small_string.h"
#include "oui_unicode.h"

namespace tex
{
	using std::string_view;

	using string = SmallString;

	constexpr static bool isSpace(char ch) { return ch >= 0 && ch <= ' '; }

	class Word
	{
		string _data;
		int _textc = 0;


		void _update_textc()
		{
			_textc = _data.size();
			while (_textc >= 0 && isSpace(_data[_textc-1]))
				--_textc;
		}
	public:
		struct Text { };
		struct Space { };
		template <class P>
		class Part
		{
			static constexpr bool _text = std::is_same_v<P, Text>;
			static constexpr bool _space = std::is_same_v<P, Space>;
			static_assert(_text || _space);

			string& _data() { return reinterpret_cast<Word*>(this)->_data; }
			int&   _textc() { return reinterpret_cast<Word*>(this)->_textc; }
			const string& _data() const { return reinterpret_cast<const Word*>(this)->_data; }
			const int&   _textc() const { return reinterpret_cast<const Word*>(this)->_textc; }

			constexpr int _offset() const
			{
				if constexpr (_text)
					return 0;
				else
					return _textc();
			}
		public:
			Part() = delete;
			Part(Part&&) = delete;
			Part(const Part&&) = delete;
			Part& operator=(Part&& p) { return operator=(p); }
			Part& operator=(const Part& p)
			{
				erase(0);
				insert(0, p);
				return *this;
			}

			Part& operator=(string_view s) { erase(0); insert(0, s); return *this; }

			int size() const 
			{
				if constexpr (_text)
					return _textc();
				else
					return _data().size() - _textc();
			}
			bool empty() const 
			{
				if constexpr (_text)
					return _textc() == 0;
				else
					return _textc() == _data().size();
			}


			auto begin() const { return _data().begin() + _offset(); }
			auto end()   const { return _data().begin() + _offset() + size(); }
			auto begin() { return _data().begin() + _offset(); }
			auto end()   { return _data().begin() + _offset() + size(); }

			char& operator[](int i)       { return begin()[i]; }
			char  operator[](int i) const { return begin()[i]; }

			auto& front() { return *begin(); }
			auto& back()  { return *(begin()+size()); }
			auto& front() const { return *begin(); }
			auto& back()  const { return *(begin()+size()); }

			void push_back(char ch)
			{
				_data().insert(_offset()+size(), { &ch, 1 });
				if constexpr (_text)
					++_textc();
			}

			void insert(int offset, string_view s)
			{
				Expects(offset >= 0 && offset <= size());
				_data().insert(_offset() + offset, s);
				if constexpr (_text)
					_textc() += s.size();
			}
			void append(string_view s) { insert(size(), s); }
			void erase(int offset, int count = -1)
			{
				if (count < 0)
					count = size() - offset;
				Expects(offset >= 0 && offset + count <= size());
				_data().erase(_offset() + offset, count);
				if constexpr (_text)
					_textc() -= count;
			}
			string extract(int offset, int count = -1)
			{
				if (count < 0)
					count = size() - offset;
				Expects(offset >= 0 && offset + count <= size());
				if constexpr (_space)
					offset += _textc();
				string result = _data().substr(offset, count);
				_data().erase(offset, count);
				if constexpr (_text)
					_textc() -= count;
				return result;
			}

			string substr(int offset, int count = -1) const
			{
				if (count < 0)
					count = size() - offset;
				Expects(offset >= 0 && offset + count <= size());
				if constexpr (_space)
					offset += _textc();
				return _data().substr(offset, count);
			}

			friend void swap(Part& a, Part& b)
			{
				string tmp = std::move(a);
				a = std::move(b);
				b = std::move(tmp);
			}

			operator string_view() const { return { begin(), static_cast<size_t>(size()) }; }

			operator string() &&
			{
				string result = _data().substr(_offset(), size());
				_data().erase(_offset(), size());
				return result;
			}
		};

		Word() noexcept { };
		template <class A>
		explicit Word(A&& data) : _data(std::forward<A>(data)) { _update_textc(); }
		Word(Word&& w) noexcept : _data(std::move(w._data)), _textc(std::exchange(w._textc, 0)) { }
		Word(const Word&) = default;

		Word& operator=(Word&& w) noexcept { _data = std::move(w._data); _textc = std::exchange(w._textc, 0); return *this; }
		Word& operator=(const Word&) = default;

		Part<Text>&   text() { return reinterpret_cast<Part<Text >&>(*this); }
		Part<Space>& space() { return reinterpret_cast<Part<Space>&>(*this); }

		const Part<Text>&   text() const { return reinterpret_cast<const Part<Text >&>(*this); }
		const Part<Space>& space() const { return reinterpret_cast<const Part<Space>&>(*this); }

		const string& data() const { return _data; }

		friend std::ostream& operator<<(std::ostream& out, const Word& w)
		{
			return out << w._data;
		}
	};

}