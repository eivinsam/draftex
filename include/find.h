#pragma once

template <class V>
struct Default
{
	V value;
};
static constexpr struct
{
	template <class V>
	using rem_cvref_t = std::remove_const_t<std::remove_reference_t<V>>;
	template <class V>
	constexpr Default<rem_cvref_t<V>> operator=(V&& value) const
	{
		return { std::forward<V>(value) };
	}
} default_value;
template <class C, class K, class V>
V find(C&& map, const K& key, const Default<V>& default_value) noexcept
{
	if (const auto found = map.find(key);
		found != map.end())
		return found->second;
	return default_value.value;
}
