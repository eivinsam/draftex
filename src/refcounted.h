#pragma once

#include <type_traits>
#include <atomic>
#include <gsl-lite.hpp>

class refcounted
{
	template <class T>
	friend class ptr;

	std::atomic_short _count;
public:
	refcounted() : _count(0) { }

	template <class T>
	class ptr
	{
		friend class refcounted;

		static_assert(std::is_base_of_v<refcounted, T>);

		T* _ptr = nullptr;

	public:
		constexpr ptr() = default;
		~ptr()
		{
			if (_ptr && _ptr->_count.fetch_sub(1) == 1)
				delete _ptr;
		}
		ptr(ptr&& other) : _ptr(other._ptr) { other._ptr = nullptr; }
		ptr(const ptr& other) : _ptr(other._ptr) { _ptr->_count += 1; }

		template <class T>
		ptr& operator=(T&& other)
		{
			this->~ptr();
			new (this) ptr(std::forward<T>(other));
			return *this;
		}

		T* get() const { return _ptr; }

		T* operator->() const { return get(); }
		T& operator*() const { return *get(); }
	};

	template <class T, class... Args>
	static auto make(Args&&... args)
	{
		ptr<T> result;
		result._ptr = new T(std::forward<Args>(args)...);
		result._ptr->_count += 1;
		return result;
	}
	template <class T>
	static auto claim(T* plain_ptr)
	{
		const auto prev_count = plain_ptr->_count.fetch_add(1);
		// outside of make() the count will only be zero when the object has not been made 
		// with make(), ie, some other kind of ownership is in place
		Expects(prev_count > 0); 
		ptr<T> result;
		result._ptr = plain_ptr;
		return result;
	}
};
