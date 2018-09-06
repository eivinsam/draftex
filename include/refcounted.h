#pragma once

#include <type_traits>
#include <atomic>
#include <gsl-lite.hpp>


//#define DEBUG_REFCOUNTED
#ifdef DEBUG_REFCOUNTED
#include <unordered_set>

class refcounted;
struct refcount_debug_data
{
	std::unordered_set<refcounted*> _register;

	void create(refcounted* p)
	{
		_register.insert(p);
	}
	void destroy(refcounted* p)
	{
		auto found = _register.find(p);
		Expects(found != _register.end());
		_register.erase(found);
	}

	~refcount_debug_data()
	{
		Expects(_register.empty());
	}
};
inline refcount_debug_data& refcount_debug()
{
	static refcount_debug_data data;
	return data;
}
#endif

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
		template <class S>
		friend class ptr;

		T* _ptr = nullptr;

		void _count_down_maybe_delete()
		{
			if (_ptr->_count.fetch_sub(1) == 1)
			{
#ifdef DEBUG_REFCOUNTED
				refcount_debug().destroy(_ptr);
#endif
				delete _ptr;
			}
		}

	public:
		constexpr ptr() = default;
		~ptr()
		{
			if (_ptr)
				_count_down_maybe_delete();
		}
		ptr(nullptr_t) : _ptr(nullptr) { }
		ptr(ptr&& other) : _ptr(other._ptr) { other._ptr = nullptr; }
		ptr(const ptr& other) : _ptr(other._ptr) { if (_ptr) _ptr->_count += 1; }
		template <class S> ptr(ptr<S>&& other) : _ptr(other._ptr) { other._ptr = nullptr; }
		template <class S> ptr(const ptr<S>& other) : _ptr(other._ptr) { if (_ptr) _ptr->_count += 1; }

		ptr& operator=(ptr&& other)
		{
			if (_ptr)
				_count_down_maybe_delete();
			_ptr = other._ptr;
			other._ptr = nullptr;
			return *this;
		}
		ptr& operator=(const ptr& other)
		{
			if (_ptr)
				_count_down_maybe_delete();
			_ptr = other._ptr;
			if (_ptr)
				_ptr->_count += 1;
			return *this;
		}

		template <class S>
		ptr& operator=(S&& other)
		{
			return operator=(ptr(std::forward<S>(other)));
		}

		T* get() const { return _ptr; }

		T* operator->() const { return get(); }
		T& operator*() const { return *get(); }

		friend constexpr bool operator==(const ptr& a, const ptr& b) { return a._ptr == b._ptr; }
		friend constexpr bool operator!=(const ptr& a, const ptr& b) { return a._ptr != b._ptr; }

		friend constexpr bool operator==(const ptr& a, T* b) { return a._ptr == b; }
		friend constexpr bool operator!=(const ptr& a, T* b) { return a._ptr != b; }

		friend constexpr bool operator==(T* a, const ptr& b) { return a == b._ptr; }
		friend constexpr bool operator!=(T* a, const ptr& b) { return a != b._ptr; }

		explicit constexpr operator bool() const { return _ptr != nullptr; }
	};

	template <class T, class... Args>
	static auto make(Args&&... args)
	{
		ptr<T> result;
		result._ptr = new T(std::forward<Args>(args)...);
		result._ptr->_count += 1;
#ifdef DEBUG_REFCOUNTED
		refcount_debug().create(result._ptr);
#endif
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
