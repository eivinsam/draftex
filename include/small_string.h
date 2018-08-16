#pragma once

#include <string_view>
#include <ostream>
#include <gsl-lite.hpp>

struct static_tests;

class SmallString
{
	friend struct static_tests;

	using view = std::string_view;

	static constexpr unsigned char _buffer_size = 15;
	static constexpr unsigned char _cap_offset = _buffer_size - 6;
	union
	{
		struct
		{
			unsigned char size;
			char data[_buffer_size];

			explicit operator bool() const { return size < _buffer_size; }
		} _small;

		struct
		{
			unsigned char cap;
			int size;
			char* data;

			explicit operator bool() const { return cap >= _buffer_size; }
		} _large;
	};

	static constexpr int _expand_cap(unsigned char cap)
	{
		Expects(cap > _buffer_size);
		cap -= _cap_offset;
		return (2 | (cap & 1)) << (cap >> 1);
	}
	static constexpr unsigned char _calc_cap(int new_size)
	{
		Expects(new_size >= 0);
		if (new_size < _buffer_size)
			return static_cast<unsigned char>(new_size);
		Expects(new_size < (3 << 29));
		for (unsigned char i = 5; ; ++i)
			if ((2 << i) > new_size)
			{
				if ((3 << (i - 1)) > new_size)
					return _cap_offset + (((i - 1) << 1) | 1);
				else
					return _cap_offset + (i << 1);
			}
	}

	void _set_size(int new_size)
	{
		if (_small)
			_small.size = gsl::narrow_cast<unsigned char>(new_size);
		else
			_large.size = new_size;
	}

public:
	using value_type = char;
	using size_type = int;
	using iterator = char*;
	using const_iterator = const char*;

	SmallString() : _small{ 0 } { }

	auto data() { return _small ? _small.data : _large.data; }
	auto data() const { return _small ? _small.data : _large.data; }
	int  size() const { return _small ? _small.size : _large.size; }
	bool empty() const { return size() == 0; }

	auto& operator[](int i)       { return data()[i]; }
	auto& operator[](int i) const { return data()[i]; }

	auto begin()       { return data(); }
	auto begin() const { return data(); }
	auto end()       { return _small ? _small.data + _small.size : _large.data + _large.size; }
	auto end() const { return _small ? _small.data + _small.size : _large.data + _large.size; }

	auto& front()       { return data()[0]; }
	auto& front() const { return data()[0]; }

	auto& back()       { return data()[size() - 1]; }
	auto& back() const { return data()[size() - 1]; }

	const char* c_str() const { return data(); }

	operator std::string_view() const
	{
		return _small ?
			view{ _small.data, _small.size } :
			view{ _large.data, gsl::narrow_cast<size_t>(_large.size) };
	}

	SmallString(view text);
	SmallString(const char* text) : SmallString(view(text)) { }
	SmallString(const std::string& text) : SmallString(view(text)) { }

	SmallString(SmallString&& other) : _small(other._small)
	{
		if (other._large)
			other._small = { 0 };
	}
	SmallString(const SmallString& other) : SmallString(view(other)) { }
	~SmallString() { if (_large) delete _large.data; }

	SmallString& operator=(SmallString&& other)
	{
		this->~SmallString();
		new (this) SmallString(std::move(other));
		return *this;
	}
	SmallString& operator=(const SmallString& other)
	{
		this->~SmallString();
		new (this) SmallString(view(other));
		return *this;
	}

	void reserve(const int min_capacity);
	void resize(const int new_size, const char fill = 0);

	void append(view text) { insert(size(), text); }
	void insert(int offset, view text);

	void push_back(char ch)
	{
		const auto new_size = size() + 1;
		reserve(new_size);
		auto e = end();
		*e = ch; ++e; *e = 0;
		_set_size(new_size);
	}

	void erase(int index, int count = -1);

	SmallString substr(int pos, int count = -1) const
	{
		return view(*this).substr(
			static_cast<size_t>(std::max(0, pos)),
			count < 0 ? view::npos : static_cast<size_t>(count));
	}

	SmallString operator+(view other) const
	{
		SmallString copy;
		copy.reserve(size() + other.size());
		copy.append(*this);
		copy.append(other);
		return copy;
	}
	SmallString operator+(const SmallString& other) const { return *this + view(other); }
	SmallString operator+(const std::string& other) const { return *this + view(other); }
	SmallString operator+(const char* other) const { return *this + view(other); }

	friend SmallString operator+(view lhs, const SmallString& rhs)
	{
		SmallString copy;
		copy.reserve(lhs.size() + rhs.size());
		copy.append(lhs);
		copy.append(rhs);
		return copy;
	}

	bool operator==(view other) const { return view(*this) == other; }
	bool operator!=(view other) const { return view(*this) != other; }

	friend bool operator==(view lhs, const SmallString& rhs) { return lhs == view(rhs); }
	friend bool operator!=(view lhs, const SmallString& rhs) { return lhs != view(rhs); }
};

struct static_tests
{
	static_assert(sizeof(SmallString) == 16);

	static_assert(SmallString::_expand_cap(SmallString::_buffer_size + 1) == 24);
	static_assert(SmallString::_expand_cap(SmallString::_buffer_size + 2) == 32);
	static_assert(SmallString::_expand_cap(SmallString::_buffer_size + 3) == 48);
	static_assert(SmallString::_expand_cap(SmallString::_buffer_size + 48) == (1 << 28));
	static_assert(SmallString::_expand_cap(SmallString::_buffer_size + 53) == (3 << 29));

	static_assert(SmallString::_calc_cap(SmallString::_expand_cap(27) - 1) == 27);
};

inline std::ostream& operator<<(std::ostream& out, const SmallString& text) { return out << std::string_view(text); }
