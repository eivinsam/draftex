#pragma once

#include <gsl-lite.hpp>

#include "refcounted.h"

namespace tex
{
	using gsl::narrow;

	template <class C, class T>
	auto count(C&& c, const T& value) { return std::count(std::begin(c), std::end(c), value); }

	template <class T>
	using Owner = refcounted::ptr<T>;

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
		template <class T, class Friend>
		class Property
		{
			friend Friend;
			T value;

		public:
		};
		template <class T, class Friend>
		class Property<T*, Friend>
		{
			friend Friend;
			T* value = nullptr;

			constexpr Property() = default;
			constexpr Property(Property&&) = default;
			constexpr Property(const Property&) = default;
			constexpr Property& operator=(Property&&) = default;
			constexpr Property& operator=(const Property&) = default;

			constexpr Property(T* value) : value(value) { }
			constexpr Property& operator=(T* new_value) { value = new_value; return *this; }
		public:
			constexpr       T* operator()() const { return value; }
			explicit constexpr operator bool() const { return static_cast<bool>(value); }

			constexpr T* operator->() const { return value; }
			constexpr T& operator*() const { return *value; }

			template <class S> constexpr bool operator==(S&& other) const { return value == other; }
			template <class S> constexpr bool operator!=(S&& other) const { return value != other; }
		};
		template <class T, class Friend>
		class Property<Owner<T>, Friend>
		{
			friend Friend;
			Owner<T> value;

			constexpr Property() = default;
			constexpr Property(Property&&) = default;
			constexpr Property(const Property&) = default;
			constexpr Property& operator=(Property&&) = default;
			constexpr Property& operator=(const Property&) = default;

			constexpr Property(Owner<T> value) : value(std::move(value)) { }
			constexpr Property& operator=(Owner<T> new_value) { value = std::move(new_value); return *this; }

		public:
			constexpr T* operator()() const { return value.get(); }
			explicit constexpr operator bool() const { return static_cast<bool>(value); }

			constexpr T* operator->() const { return value.get(); }
			constexpr T& operator*() const { return *value; }

			template <class S> constexpr bool operator==(S&& other) const { return value == other; }
			template <class S> constexpr bool operator!=(S&& other) const { return value != other; }
		};
	}
}
