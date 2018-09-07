#pragma once

#include <type_traits>
#include <atomic>
#include <gsl-lite.hpp>



//#define DEBUG_REFCOUNTED
#ifdef DEBUG_REFCOUNTED
#include <unordered_set>
namespace intrusive
{
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
}
#endif

namespace intrusive
{
	using std::move;

	struct raw
	{
		template <class T>
		class ptr
		{
		protected:
			T* _ptr = nullptr;
		public:
			constexpr ptr() = default;
			constexpr ptr(T* p) : _ptr(p) { }
			constexpr ptr(const ptr&) = default;
			constexpr ptr(ptr&& other) : _ptr(std::exchange(other._ptr, nullptr)) { }

			constexpr ptr& operator=(const ptr&) = default;
			constexpr ptr& operator=(ptr&& other) {	_ptr = std::exchange(other._ptr, nullptr); }

			constexpr T* get() const { return _ptr; }

			T* operator->() const { return get(); }
			T& operator*() const { return *get(); }

			friend constexpr bool operator==(const ptr& a, const ptr& b) { return a._ptr == b._ptr; }
			friend constexpr bool operator!=(const ptr& a, const ptr& b) { return a._ptr != b._ptr; }

			template <class S> friend constexpr bool operator==(const ptr& a, S* b) { return a._ptr == b; }
			template <class S> friend constexpr bool operator!=(const ptr& a, S* b) { return a._ptr != b; }

			template <class S> friend constexpr bool operator==(S* a, const ptr& b) { return a == b._ptr; }
			template <class S> friend constexpr bool operator!=(S* a, const ptr& b) { return a != b._ptr; }

			explicit constexpr operator bool() const { return _ptr != nullptr; }
		};
	};

	class refcount
	{
		template <class T>
		friend class ptr;

		std::atomic_short _count;
	public:
		refcount() : _count(0) { }

		template <class T>
		class ptr : public raw::ptr<T>
		{
			using raw::ptr<T>::_ptr;

			friend class refcount;
			template <class S>
			friend class ptr;

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
			ptr(nullptr_t) : raw::ptr<T>(nullptr) { }
			ptr(ptr&& other) : raw::ptr<T>(other._ptr) { other._ptr = nullptr; }
			ptr(const ptr& other) : raw::ptr<T>(other._ptr) { if (_ptr) _ptr->_count += 1; }
			template <class S> ptr(ptr<S>&& other) : raw::ptr<T>(other._ptr) { other._ptr = nullptr; }
			template <class S> ptr(const ptr<S>& other) : raw::ptr<T>(other._ptr) { if (_ptr) _ptr->_count += 1; }

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

	template <class, class>
	class list_element;
	template <class, class, auto>
	class list;

	template <class T, class P>
	class list_element
	{
		template <class, class, auto>
		friend class list;
		refcount::ptr<T> _next;
		raw::ptr<T> _prev;
		raw::ptr<P> _parent;
	public:
		T* next() const { return _next.get(); }
		T* prev() const { return _prev.get(); }

		P* operator()() const { return _parent.get(); }

		P* operator->() const { return _parent.get(); }
	};

	template <class T, class P, auto EM>
	class list
	{
		using E = std::remove_reference_t<decltype(std::declval<T*>()->*EM)>;

		refcount::ptr<T> _first;
		raw::ptr<T> _last;
		template <class Ptr, class = decltype(std::declval<Ptr>().get())>
		static E* _e(const Ptr& p) { return &(p.get()->*EM); }
		static E* _e(T* p) { return &(p->*EM); }
	public:
		~list()
		{
			while (_last)
				pop_back();
		}
		list(const list&) = delete;
		list& operator=(const list&) = delete;

		void pop_back()
		{
			auto le = _e(_last);
			le->_parent = nullptr;
			_last = std::exchange(le->_prev, nullptr);
			if (_last)
				_e(_last)->_next = nullptr;
		}

		template <class S>
		S* append(refcount::ptr<S> e)
		{
			const auto raw_e = e.get();
			if (!_last)
			{
				_first = move(e);
				_last = _first;
			}
			else
			{
				const auto old_last = _last;
				_e(old_last)->_next = move(e);
				_last = _e(old_last)->_next;
				_e(_last)->_prev = old_last;
			}
			_e(_last)->_parent = static_cast<P*>(this);
			return raw_e;
		}

		template <class S>
		S* insert_before(T* next, refcount::ptr<S> e)
		{
			Expects(next != nullptr);
			const auto raw_e = e.get();
			const auto ee = _e(e);
			if (const auto next_prev = _e(next)->_prev)
			{
				ee->_next = move(_e(next_prev)->_next);
				ee->_prev = next_prev;
				_e(next)->_prev = e.get();
				_e(next_prev)->_next = move(e);
			}
			else
			{
				ee->_next = move(_first);
				ee->_prev = nullptr;
				_e(next)->_prev = e.get();
				_first = move(e);
			}
			_e(_e(next)->_prev)->_parent = static_cast<P*>(this);
			return raw_e;
		}

		template <class S>
		S* insert_after(T* prev, refcount::ptr<S> e)
		{
			return _e(prev)->_next ?
				insert_before(_e(prev)->_next.get(), move(e)) :
				append(move(e));
		}

		refcount::ptr<T> detach(T* e)
		{
			const auto ee = _e(e);
			Expects(ee->_parent == this);

			auto& prev_next = ee->_prev ? _e(ee->_prev)->_next : _first;
			auto& next_prev = ee->_next ? _e(ee->_next)->_prev : _last;

			auto result = move(prev_next);

			prev_next = move(ee->_next);
			next_prev = move(ee->_prev);

			ee->_parent = nullptr;

			return result;
		}

		void remove(T* e)
		{
			(void)detach(e);
		}

		constexpr bool empty() const { return _first == _last; }

		struct sentinel {};

		class iterator
		{
			friend class list;
		protected:
			T* _p;
			constexpr iterator(T* p) : _p(p) { }
		public:
			iterator& operator++() { _p = _e(_p)->_next.get(); }

			T* operator->() const { return _p; }
			T& operator*() const { return *_p; }

			constexpr bool operator==(sentinel) const { return _p == nullptr; }
			constexpr bool operator!=(sentinel) const { return _p != nullptr; }
		};

		class reverse_iterator : public iterator
		{
			friend class list;

			using iterator::iterator;
			using iterator::_p;
		public:
			reverse_iterator& operator++() { _p = _e(_p)->_prev.get(); }
		};


		iterator begin() const { return { _first.get() }; }
		sentinel end() const { return {}; };

		iterator rbegin() const { return { _last.get() }; }
		sentinel rend() const { return {}; }
	};
}
