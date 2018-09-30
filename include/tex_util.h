#pragma once

#pragma warning(push)
#pragma warning(disable: 4127)
#include <gsl-lite.hpp>
#pragma warning(pop)

#include "intrusive.h"

namespace tex
{
	using gsl::narrow;

	template <class C, class T>
	auto count(C&& c, const T& value) { return std::count(std::begin(c), std::end(c), value); }

	template <class T>
	using Owner = intrusive::refcount::ptr<T>;

	template <class T>
	tex::Owner<T> claim(T* ptr) { return intrusive::refcount::claim(ptr); }
	template <class T>
	auto claim_mutable(T* ptr) { return intrusive::refcount::claim(intrusive::as_mutable(ptr)); }


	template <class To, class From>
	inline auto as(From* from) noexcept
	{
		return dynamic_cast<std::conditional_t<std::is_const_v<From>, const To*, To*>>(from);
	}
	template <class To, class From>
	inline auto& as(From& from) noexcept
	{
		return dynamic_cast<std::conditional_t<std::is_const_v<From>, const To&, To&>>(from);
	}

	template <auto... Values>
	struct is_any_of_t
	{
		template <class T>
		constexpr bool operator()(const T& value) const
		{
			return ((value == Values) || ...);
		}
	};
	template <auto... Values>
	constexpr is_any_of_t<Values...> is_any_of = {};


	namespace details
	{

		//template <class T, class Friend>
		//class Property
		//{
		//	friend Friend;
		//	T value;
		//public:
		//
		//	T& operator() noexcept { return value; }
		//	const T& operator() const noexcept { return value; }
		//
		//
		//};
		//template <class T, class Friend>
		//class Property<T*, Friend>
		//{
		//	friend Friend;
		//	T* value = nullptr;
		//
		//	constexpr Property() = default;
		//	constexpr Property(Property&&) = default;
		//	constexpr Property(const Property&) = default;
		//	constexpr Property& operator=(Property&&) = default;
		//	constexpr Property& operator=(const Property&) = default;
		//
		//	constexpr Property(T* value) : value(value) { }
		//	constexpr Property& operator=(T* new_value) { value = new_value; return *this; }
		//public:
		//	constexpr       T* operator()() const { return value; }
		//	explicit constexpr operator bool() const { return static_cast<bool>(value); }
		//
		//	constexpr T* operator->() const { return value; }
		//	constexpr T& operator*() const { return *value; }
		//
		//	template <class S> constexpr bool operator==(S&& other) const { return value == other; }
		//	template <class S> constexpr bool operator!=(S&& other) const { return value != other; }
		//};
		//template <class T, class Friend>
		//class Property<Owner<T>, Friend>
		//{
		//	friend Friend;
		//	Owner<T> value;
		//
		//	constexpr Property() = default;
		//	constexpr Property(Property&&) = default;
		//	constexpr Property(const Property&) = default;
		//	constexpr Property& operator=(Property&&) = default;
		//	constexpr Property& operator=(const Property&) = default;
		//
		//	constexpr Property(Owner<T> value) : value(std::move(value)) { }
		//	constexpr Property& operator=(Owner<T> new_value) { value = std::move(new_value); return *this; }
		//
		//public:
		//	constexpr T* operator()() const { return value.get(); }
		//	explicit constexpr operator bool() const { return static_cast<bool>(value); }
		//
		//	constexpr T* operator->() const { return value.get(); }
		//	constexpr T& operator*() const { return *value; }
		//
		//	template <class S> constexpr bool operator==(S&& other) const { return value == other; }
		//	template <class S> constexpr bool operator!=(S&& other) const { return value != other; }
		//};
	}
}
