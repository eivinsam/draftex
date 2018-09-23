#pragma once

#include <string_view>
#include <ostream>
#include <gsl-lite.hpp>

#pragma warning(push)
#pragma warning(disable: 26485 26481 26409 26401)

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

			explicit constexpr operator bool() const { return size < _buffer_size; }
		} _small;

		struct
		{
			unsigned char cap;
			int size;
			char* data;

			explicit constexpr operator bool() const { return cap >= _buffer_size; }
		} _large;
	};

	static constexpr int _expand_cap(unsigned char cap)
	{
		if (cap < _buffer_size)
			return _buffer_size;
		cap -= _cap_offset;
		return (2 | (cap & 1)) << (cap >> 1);
	}
	static constexpr unsigned char _calc_cap(int new_size)
	{
		Expects(new_size >= 0);
		if (new_size < _buffer_size)
			return gsl::narrow<unsigned char>(new_size);
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
			_small.size = gsl::narrow<unsigned char>(new_size);
		else
			_large.size = new_size;
	}
	void _reserve_unchecked(const int min_capacity);

	template <class T, class R>
	using if_str = std::enable_if_t<std::is_convertible_v<T, view>, R>;

	template <class T>
	static constexpr view fwd(T&& value) { return { std::forward<T>(value) }; }
public:
	using value_type = char;
	using size_type = int;
	using iterator = char*;
	using const_iterator = const char*;

	SmallString() : _small{ 0 } { }

	constexpr auto data()       noexcept { return _small ? _small.data : _large.data; }
	constexpr auto data() const noexcept { return _small ? _small.data : _large.data; }
	constexpr int  size() const noexcept { return _small ? _small.size : _large.size; }
	constexpr bool empty() const noexcept { return size() == 0; }

	constexpr auto& operator[](int i)       noexcept { return data()[i]; }
	constexpr auto& operator[](int i) const noexcept { return data()[i]; }

	constexpr auto begin()       noexcept { return data(); }
	constexpr auto begin() const noexcept { return data(); }
	constexpr auto end()       noexcept { return _small ? _small.data + _small.size : _large.data + _large.size; }
	constexpr auto end() const noexcept { return _small ? _small.data + _small.size : _large.data + _large.size; }

	constexpr auto& front()       noexcept { return data()[0]; }
	constexpr auto& front() const noexcept { return data()[0]; }

	constexpr auto& back()       noexcept { return data()[size() - 1]; }
	constexpr auto& back() const noexcept { return data()[size() - 1]; }

	constexpr const char* c_str() const noexcept { return data(); }

	constexpr operator std::string_view() const noexcept
	{
		return _small ?
			view{ _small.data, _small.size } :
			view{ _large.data, gsl::narrow_cast<size_t>(_large.size) };
	}

	SmallString(view text);
	SmallString(const char* text) : SmallString(view(text)) { }
	SmallString(const std::string& text) : SmallString(view(text)) { }

	constexpr SmallString(SmallString&& other) noexcept : _small(other._small)
	{
		other._small.size = 0;
		other._small.data[0] = 0;
	}
	SmallString(const SmallString& other) : SmallString(view(other)) { }
	~SmallString() { if (_large) delete _large.data; }

	SmallString& operator=(SmallString&& other) noexcept
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

	void reserve(const int min_capacity)
	{
		if (min_capacity < _buffer_size || 
			min_capacity < _expand_cap(_large.cap))
			return;
		_reserve_unchecked(min_capacity);
	}
	void resize(const int new_size, const char fill = 0);

	void append(view text) { insert(size(), text); }
	void insert(int offset, view text);

	void push_back(char ch)
	{
		const auto new_size = size() + 1;
		reserve(new_size);
		auto e = end();
		Ensures(e != nullptr);
		*e = ch; ++e; *e = 0;
		_set_size(new_size);
	}

	void erase(int index, int count = -1) noexcept;

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


	template <class S, class T> friend constexpr if_str<S, if_str<T, bool>> operator==(S&& a, T&& b) noexcept { return fwd<S>(a) == fwd<T>(b); }
	template <class S, class T> friend constexpr if_str<S, if_str<T, bool>> operator!=(S&& a, T&& b) noexcept { return fwd<S>(a) != fwd<T>(b); }
	template <class S, class T> friend constexpr if_str<S, if_str<T, bool>> operator< (S&& a, T&& b) noexcept { return fwd<S>(a) <  fwd<T>(b); }
	template <class S, class T> friend constexpr if_str<S, if_str<T, bool>> operator<=(S&& a, T&& b) noexcept { return fwd<S>(a) <= fwd<T>(b); }
	template <class S, class T> friend constexpr if_str<S, if_str<T, bool>> operator>=(S&& a, T&& b) noexcept { return fwd<S>(a) >= fwd<T>(b); }
	template <class S, class T> friend constexpr if_str<S, if_str<T, bool>> operator> (S&& a, T&& b) noexcept { return fwd<S>(a)  > fwd<T>(b); }
};

#pragma warning(pop)

inline std::ostream& operator<<(std::ostream& out, const SmallString& text) { return out << std::string_view(text); }
