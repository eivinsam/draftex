#pragma once

#include <gsl-lite.hpp>

namespace tex
{
	using gsl::narrow;

	template <class C, class T>
	auto count(C&& c, const T& value) { return std::count(std::begin(c), std::end(c), value); }

	template <class T>
	using Owner = std::unique_ptr<T>;

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

	template <auto... Values, class T>
	constexpr bool is_any_of(const T& value)
	{
		return ((value == Values) || ...);
	}


	namespace details
	{
		template <class T, class Friend>
		class Property
		{
			friend Friend;
			T value;

		public:
			constexpr operator T() const { return value; }
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
			constexpr operator T*() const { return value; }
			constexpr operator const T*() const { return value; }
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

			constexpr Owner<T>& owning() { return value; }
		public:
			constexpr operator T*() const { return value.get(); }
			constexpr operator const T*() const { return value.get(); }
			explicit constexpr operator bool() const { return static_cast<bool>(value); }

			constexpr T* operator->() const { return value.get(); }
			constexpr T& operator*() const { return *value; }

			template <class S> constexpr bool operator==(S&& other) const { return value == other; }
			template <class S> constexpr bool operator!=(S&& other) const { return value != other; }
		};
	}
}
