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
	std::unordered_set<void*> _register;

	void create(void* p)
	{
		_register.insert(p);
	}
	void destroy(void* p)
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

	constexpr struct {} nocheck = {};

	template <class From, class To>
	using if_convertible_t = std::enable_if_t<std::is_convertible_v<From, To>>;

	template <class T>
	class nonull
	{
		template <class S>
		friend class nonull;
		T _ptr;


		constexpr nonull(T ptr, decltype(nocheck)) : _ptr(move(ptr)) { }
	public:
		nonull() = delete;
		template <class S, class = if_convertible_t<S, T>>
		constexpr nonull(S&& ptr) : _ptr(std::forward<S>(ptr)) { Expects(_ptr != nullptr); }
		template <class S>
		constexpr nonull(nonull<S> other) : _ptr(move(other._ptr)) { }

		friend constexpr bool operator==(const nonull& a, const nonull& b) { return a._ptr == b._ptr; }
		friend constexpr bool operator!=(const nonull& a, const nonull& b) { return a._ptr != b._ptr; }
		template <class S> friend constexpr bool operator==(const nonull& a, S b) { return a._ptr == b; }
		template <class S> friend constexpr bool operator!=(const nonull& a, S b) { return a._ptr != b; }
		template <class S> friend constexpr bool operator==(S a, const nonull& b) { return a == b._ptr; }
		template <class S> friend constexpr bool operator!=(S a, const nonull& b) { return a != b._ptr; }

		constexpr auto get() const { return nonull<decltype(_ptr.get())>{ _ptr.get(), nocheck }; }
		
		constexpr decltype(auto) operator->() const { return _ptr.operator->(); }
		constexpr decltype(auto) operator*()  const { return _ptr.operator*(); }

		template <class S, class = if_convertible_t<S, T>> constexpr operator S()      & { return _ptr; }
		template <class S, class = if_convertible_t<S, T>> constexpr operator S() const& { return _ptr; }

		[[nodiscard]] friend T extract(nonull&& p) { return move(p._ptr); }

		explicit constexpr operator bool() const { return _ptr != nullptr; }
	};

	template <class T>
	class nonull<T*>
	{
		template <class S>
		friend class nonull;
		T* _ptr;
		constexpr nonull(T* ptr, decltype(nocheck)) : _ptr(ptr) { }
	public:
		nonull() = delete;
		template <class S>
		constexpr nonull(S* ptr) : _ptr{ ptr } { Expects(_ptr != nullptr); }
		template <class S>
		constexpr nonull(nonull<S*> other) : _ptr{ other._ptr } { }

		friend constexpr bool operator==(const nonull& a, const nonull& b) { return a._ptr == b._ptr; }
		friend constexpr bool operator!=(const nonull& a, const nonull& b) { return a._ptr != b._ptr; }
		template <class S> friend constexpr bool operator==(const nonull& a, S b) { return a._ptr == b; }
		template <class S> friend constexpr bool operator!=(const nonull& a, S b) { return a._ptr != b; }
		template <class S> friend constexpr bool operator==(S a, const nonull& b) { return a == b._ptr; }
		template <class S> friend constexpr bool operator!=(S a, const nonull& b) { return a != b._ptr; }

		constexpr decltype(auto) operator->() const { return _ptr; }
		constexpr decltype(auto) operator*()  const { return *_ptr; }

		template <class S>
		constexpr operator S*() const { return _ptr; }

		[[nodiscard]] friend T* extract(nonull&& n) { return std::exchange(n._ptr, nullptr); }

		explicit constexpr operator bool() const { return _ptr != nullptr; }
	};
	template <class T>
	nonull(T&&)->nonull<std::remove_reference_t<T>>;

	template <class To, class From>
	auto as(const nonull<From>& from) { return as<To>(static_cast<From>(from)); }


#pragma warning(push)
#pragma warning(disable: 26432 26495)
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
			constexpr ptr(ptr&& other) noexcept : _ptr(std::exchange(other._ptr, nullptr)) { }
			constexpr ptr(const ptr& other) noexcept : _ptr(other._ptr) { }
			template <class S>
			constexpr ptr(nonull<S*> p) : _ptr(p) { }

			constexpr ptr& operator=(ptr&& other) noexcept { _ptr = std::exchange(other._ptr, nullptr); }
			constexpr ptr& operator=(const ptr& other) noexcept { _ptr = other._ptr; }

			constexpr T* get() const { return _ptr; }

			constexpr T* operator->() const { return get(); }
			constexpr T& operator*() const { return *get(); }

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

		mutable std::atomic_short _count;
	public:
		refcount() : _count(0) { }

		template <class T>
		class ptr
		{
			friend class refcount;
			template <class S>
			friend class ptr;

			T* _ptr = nullptr;

			void _count_down_maybe_delete() noexcept
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
			constexpr ptr() noexcept = default;
			~ptr()
			{
				if (_ptr)
					_count_down_maybe_delete();
			}
			ptr(nullptr_t) noexcept : _ptr(nullptr) { }
			constexpr ptr(ptr&& other) noexcept : _ptr(std::exchange(other._ptr, nullptr)) { }
			     ptr(const ptr& other) noexcept : _ptr(other._ptr) { if (_ptr) _ptr->_count += 1; }
			
			template <class S, class = if_convertible_t<S*, T*>>
			constexpr ptr(ptr<S>&& other) noexcept : _ptr(std::exchange(other._ptr, nullptr)) { }
			
			template <class S, class = if_convertible_t<S*, T*>>
			ptr(const ptr<S>& other) noexcept : _ptr(other._ptr) { if (_ptr) _ptr->_count += 1; }
			
			template <class S, class = if_convertible_t<S*, T*>>
			ptr(const nonull<ptr<S>>& other) noexcept : _ptr(other.get()) { _ptr->_count += 1; }

			ptr& operator=(ptr&& other) noexcept
			{
				if (_ptr)
					_count_down_maybe_delete();
				_ptr = other._ptr;
				other._ptr = nullptr;
				return *this;
			}
			ptr& operator=(const ptr& other) noexcept
			{
				if (_ptr)
					_count_down_maybe_delete();
				_ptr = other._ptr;
				if (_ptr)
					_ptr->_count += 1;
				return *this;
			}

			constexpr T* get() const { return _ptr; }

			constexpr T* operator->() const { return get(); }
			constexpr T& operator*() const { return *get(); }

			friend constexpr bool operator==(const ptr& a, const ptr& b) { return a._ptr == b._ptr; }
			friend constexpr bool operator!=(const ptr& a, const ptr& b) { return a._ptr != b._ptr; }

			template <class S> friend constexpr bool operator==(const ptr& a, S b) { return a._ptr == b; }
			template <class S> friend constexpr bool operator!=(const ptr& a, S b) { return a._ptr != b; }

			template <class S> friend constexpr bool operator==(S a, const ptr& b) { return a == b._ptr; }
			template <class S> friend constexpr bool operator!=(S a, const ptr& b) { return a != b._ptr; }

			explicit constexpr operator bool() const { return _ptr != nullptr; }
		};
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 26409)
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
#pragma warning(pop)
		template <class T>
		static auto claim(T* plain_ptr) noexcept
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

	template <class T>
	class list_element<T, void>
	{
		template <class, class, auto>
		friend class list;
		mutable refcount::ptr<T> _next;
		mutable raw::ptr<T> _prev;
	public:
		constexpr T* next() { return _next.get(); }
		constexpr T* prev() { return _prev.get(); }

		constexpr const T* next() const { return _next.get(); }
		constexpr const T* prev() const { return _prev.get(); }
	};

	template <class T, class P>
	class list_element : public list_element<T, void>
	{
		template <class, class, auto>
		friend class list;
		mutable raw::ptr<P> _parent;
	public:

		constexpr P* operator()() { return _parent.get(); }
		constexpr P* operator->() { return _parent.get(); }

		constexpr const P* operator()() const { return _parent.get(); }
		constexpr const P* operator->() const { return _parent.get(); }
	};

	template <class T> inline T* as_mutable(const T* a) { return const_cast<T*>(a); }
	template <class T> inline T& as_mutable(const T& a) { return const_cast<T&>(a); }

	template <class T, class P, auto EM>
	class list
	{
		using const_list = list<std::add_const_t<T>, P, EM>;
		static constexpr bool P_void = std::is_same_v<P, void>;
		using E = std::remove_reference_t<decltype(std::declval<T*>()->*EM)>;

		refcount::ptr<T> _first;
		raw::ptr<T> _last;
		template <class Ptr, class = decltype(std::declval<Ptr>().get())>
		static constexpr E* _e(const Ptr& p) noexcept { Expects(p != nullptr);  return &((*p).*EM); }
		static constexpr E* _e(nonull<T*> p) noexcept { return &((*p).*EM); }
	public:
		constexpr list() = default;
		~list()
		{
			while (_last)
				pop_back();
		}
		list(list&&) = default;
		list(const list&) = delete;
		list& operator=(list&&) = default;
		list& operator=(const list&) = delete;

		void pop_back() noexcept
		{
			auto le = _e(_last);
			if constexpr (!P_void)
				le->_parent = nullptr;
			_last = std::exchange(le->_prev, nullptr);
			(_last ? _e(_last)->_next : _first) = nullptr;
		}

		template <class S>
		nonull<S*> append(nonull<refcount::ptr<S>> e) noexcept
		{
			const auto raw_e = e.get();
			const auto ee = _e(raw_e);
			if constexpr (!P_void)
			{
				Expects(ee->_parent == nullptr);
			}
			Expects(ee->_prev == nullptr);
			Expects(ee->_next == nullptr);
			if (!_last)
			{
				_first = move(e);
				_last = _first.get();
			}
			else
			{
				const auto old_last = _last;
				_e(old_last)->_next = move(e);
				_last = _e(old_last)->_next.get();
				_e(_last)->_prev = old_last;
			}
			if constexpr (!P_void)
			{
				_e(_last)->_parent = static_cast<P*>(this);
			}
			return raw_e;
		}
		template <class S>
		nonull<S*> append(refcount::ptr<S> e) noexcept { return append(nonull{ move(e) }); }

		template <class S>
		nonull<S*> insert_before(nonull<T*> next, nonull<refcount::ptr<S>> e) noexcept
		{
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
			if constexpr (!P_void)
				_e(_e(next)->_prev)->_parent = static_cast<P*>(this);
			return raw_e;
		}

		template <class S>
		nonull<S*> insert_after(nonull<T*> prev, nonull<refcount::ptr<S>> e) noexcept
		{
			return _e(prev)->_next ?
				insert_before(_e(prev)->_next.get(), move(e)) :
				append(move(e));
		}

		nonull<refcount::ptr<T>> detach(nonull<T*> e) noexcept
		{
			const auto ee = _e(e);
			if constexpr (P_void)
			{
				Expects(ee->_prev || e == _first);
				Expects(ee->_next || e == _last);
			}
			else
			{
				Expects(ee->_parent == this);
				ee->_parent = nullptr;
			}

			auto& prev_next = ee->_prev ? _e(ee->_prev)->_next : _first;
			auto& next_prev = ee->_next ? _e(ee->_next)->_prev : _last;

			auto result = move(prev_next);

			prev_next = move(ee->_next);
			next_prev = move(ee->_prev);

			return result;
		}

		void remove(T* e) noexcept
		{
			(void)detach(e);
		}

		constexpr bool empty() const { return _first == nullptr; }

		struct sentinel {};
		struct reverse_sentinel {};

		class iterator
		{
			friend class list;
			friend class const_list;
		protected:
			T* _p;
			constexpr iterator(T* p) : _p(p) { }
		public:
			iterator& operator++() noexcept { _p = _e(_p)->_next.get(); return *this; }
			iterator operator++(int) noexcept { auto copy = *this; ++*this; return copy; }

			constexpr T* operator->() const { return _p; }
			constexpr T& operator*() const { return *_p; }

			constexpr bool operator==(sentinel) const { return _p == nullptr; }
			constexpr bool operator!=(sentinel) const { return _p != nullptr; }
		};

		class reverse_iterator : public iterator
		{
			friend class list;
			friend class const_list;

			using iterator::iterator;
			using iterator::_p;
		public:
			reverse_iterator& operator++() { _p = _e(_p)->_prev.get(); }
		};


		constexpr iterator begin() { return { _first.get() }; }
		constexpr sentinel end()   { return {}; };
		constexpr typename const_list::iterator begin() const { return { _first.get() }; }
		constexpr typename const_list::sentinel end()   const { return {}; }

		constexpr reverse_iterator rbegin() { return { _last.get() }; }
		constexpr reverse_sentinel rend()   { return {}; }
		constexpr typename const_list::reverse_iterator rbegin() const { return { _last.get() }; }
		constexpr typename const_list::reverse_sentinel rend()   const { return {}; }
	};
}
