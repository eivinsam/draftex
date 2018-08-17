#include <small_string.h>

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
		return;
	}
	reserve(new_size);
	const auto dst = data();
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
	reserve(new_size);
	if (offset < size())
	{
		const auto old_end = end();
		const auto new_end = old_end + text_size;
		for (int i = 0; i < text_size; ++i)
			new_end[i] = old_end[i];
	}
	if (_small)
	{
		memcpy(_small.data + _small.size, text.data(), text.size());
		_small.size = static_cast<unsigned char>(new_size);
		_small.data[_small.size] = 0;
	}
	else
	{
		memcpy(_large.data + _large.size, text.data(), text.size());
		_large.size = new_size;
		_large.data[_large.size] = 0;
	}
}

void SmallString::erase(int index, int count)
{
	Expects(index >= 0 && index < size());
	if (count == 0)
		return;
	if (count < 0 || index + count > size())
		count = size() - index;

	for (auto dst = data() + index, src = dst + count, src_end = end(); src < src_end; ++dst, ++src)
		dst = src;
	if (_small)
		_small.size -= static_cast<unsigned char>(count);
	else
		_large.size -= count;
}
