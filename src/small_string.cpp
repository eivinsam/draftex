#include <small_string.h>

#pragma warning(disable: 26409 26485 26403 26446 26472 26481 26482)

SmallString::SmallString(view text)
{
	_large.cap = _calc_cap(text.size());
	if (_small)
		memcpy(_small.data, text.data(), text.size());
	else
	{
		_large.size = text.size();
		_large.data = new char[_expand_cap(_large.cap)];
		memcpy(_large.data, text.data(), text.size());
	}
	*end() = 0;
}

void SmallString::_reserve_unchecked(const int min_capacity)
{
	const auto old = view(*this);
	const auto new_cap = _calc_cap(min_capacity);
	const auto new_data = new char[_expand_cap(new_cap)];
	memcpy(new_data, old.data(), old.size() + 1);
	_large = { new_cap, gsl::narrow_cast<int>(old.size()), new_data };
}

void SmallString::resize(const int new_size, const char fill)
{
	if (size() >= new_size)
	{
		if (_small)
			_small.size = static_cast<unsigned char>(new_size);
		else
			_large.size = new_size;
		operator[](new_size) = 0;
		return;
	}
	reserve(new_size);
	const auto dst = data();
	Expects(dst != nullptr);
	for (auto i = size(); i < new_size; ++i)
		dst[i] = fill;
	_set_size(new_size);
	*end() = 0;
}

void SmallString::insert(int offset, view text)
{
	const auto text_size = gsl::narrow_cast<int>(text.size());
	const auto new_size = size() + text_size;
	Expects(new_size >= 0);
	Expects(offset >= 0 && offset <= size());
	reserve(new_size);
	if (offset < size())
	{
		const auto old_end = end();
		Expects(old_end != nullptr);
		const auto new_end = old_end + text_size;
		const auto N = size() - offset;

		for (int i = 1; i <= N; ++i)
			new_end[-i] = old_end[-i];
	}
	if (_small)
	{
		memcpy(_small.data + offset, text.data(), text.size());
		_small.size = static_cast<unsigned char>(new_size);
		_small.data[_small.size] = 0;
	}
	else
	{
		memcpy(_large.data + offset, text.data(), text.size());
		_large.size = new_size;
		_large.data[_large.size] = 0;
	}
}

void SmallString::erase(int index, int count) noexcept
{
	Expects(index >= 0 && index < size());
	if (count == 0)
		return;
	if (count < 0 || index + count > size())
		count = size() - index;

	char* const dst = data() + index;
	Expects(dst != nullptr);
	const char* const src = dst + count;
	Expects(src != nullptr);
	const auto N = size() - (index + count);

	for (int i = 0; i <= N; ++i)
		dst[i] = src[i];
	if (_small)
		_small.size -= static_cast<unsigned char>(count);
	else
		_large.size -= count;
}

struct static_tests
{
	static_assert(sizeof(SmallString) == 16);

	static constexpr auto foo = SmallString::_calc_cap(15);
	static_assert(SmallString::_expand_cap(SmallString::_buffer_size + 1) == 24);
	static_assert(SmallString::_expand_cap(SmallString::_buffer_size + 2) == 32);
	static_assert(SmallString::_expand_cap(SmallString::_buffer_size + 3) == 48);
	static_assert(SmallString::_expand_cap(SmallString::_buffer_size + 48) == (1 << 28));
	static_assert(SmallString::_expand_cap(SmallString::_buffer_size + 53) == (3 << 29));

	static_assert(SmallString::_calc_cap(SmallString::_expand_cap(27) - 1) == 27);
};
