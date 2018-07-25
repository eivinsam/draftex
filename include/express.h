#pragma once

#include <functional>

namespace xpr
{
	template <class T>
	using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

	struct End {};

	template <class T>
	struct pred : T
	{
		constexpr pred(T t) : T{ std::move(t) } { }
	};

	template <class T>
	struct co
	{
		template <class S>
		constexpr bool operator()(S&& s) const {
			return static_cast<const T&>(*
				this)(std::forward<S>(s));
		}
	};

	template <class P>
	struct neg : co<neg<P>>
	{
		P pos;

		constexpr neg(P pos) : pos(pos) { }

		template <class S>
		constexpr bool operator()(S&& s) const { return !pos(std::forward<S>(s)); }
	};

	struct Equal
	{
		template <class T>
		struct To : co<To<T>>
		{
			T value;

			constexpr To(T value) : value(value) { }

			template <class S>
			constexpr bool operator()(S&& s) const { return s == value; }

			constexpr neg<To> operator!() const { return { *this }; }
		};
		template <class T>
		constexpr To<T> to(T value) const { return { value }; };
	};
	static constexpr Equal equal;

	struct At
	{
		template <class T>
		struct Most : co<Most<T>>
		{
			T value;

			constexpr Most(T value) : value(value) { }

			template <class S>
			constexpr bool operator()(S&& s) const { return s <= value; }
		};
		template <class T>
		constexpr Most<T> most(T value) const { return { value }; }
	};
	static constexpr At at;


	template <class T>
	constexpr pred<T> is(co<T>&& comp) { return { static_cast<T&&>(comp) }; }
	template <class T>
	constexpr pred<T> are(co<T>&& comp) { return { static_cast<T&&>(comp) }; }


	template <class IT, class S>
	struct Range;

	template <class P, class IT, class S>
	class match_iterator : P
	{
		Range<IT, S> _r;

		constexpr void _skip_mismatches()
		{
			while (_r.begin() != _r.end() && !(*this)(*_r.first))
				++_r.first;
		}
	public:
		constexpr match_iterator(P pred, Range<IT, S> r) :
			P(std::move(pred)), _r(std::move(r))
		{
			_skip_mismatches();
		}

		constexpr match_iterator& operator++() { ++_r.first; _skip_mismatches(); return *this; }

		constexpr decltype(auto) operator*() { return *_r.first; }
		constexpr const IT& operator->() { return _r.first; }

		constexpr bool operator==(End) const { return _r.begin() == _r.end(); }
		constexpr bool operator!=(End) const { return _r.begin() != _r.end(); }
	};

	template <class IT, class S>
	struct Range
	{
		IT first;
		S last;

		constexpr Range(IT first, S last) : first(std::move(first)), last(std::move(last)) { }

		constexpr IT begin() const { return first; }
		constexpr S end() const { return last; }

		constexpr bool empty() const { return first == last; }

		template <class P>
		constexpr Range<match_iterator<remove_cvref_t<P>, IT, S>, End> that(P&& p)
		{
			return { {p, {first, last}}, {} };
		}

		template <class V>
		constexpr bool contains(const V& value)
		{
			for (auto&& e : *this)
				if (e == value)
					return true;
			return false;
		}
	};
	template <class IT>
	struct Range<IT, End>
	{
		IT first;

		constexpr Range(IT first, End) : first(std::move(first)) { }

		constexpr IT begin() const { return first; }
		constexpr End end() const { return {}; }

		constexpr bool empty() const { return first == End{}; }

		template <class P>
		constexpr Range<match_iterator<remove_cvref_t<P>, IT, End>, End> that(P&& p)
		{
			return { {p, {first, {}}}, {} };
		}

		template <class V>
		constexpr bool contains(const V& value)
		{
			for (auto&& e : *this)
				if (e == value)
					return true;
			return false;
		}
	};

	template <class C>
	using BeginType = decltype(std::begin(std::declval<C>()));
	template <class C>
	using EndType = decltype(std::end(std::declval<C>()));

	template <class C>
	using RangeType = Range<BeginType<C>, EndType<C>>;

	template <class IT>
	class From
	{
		IT _it;
	public:
		constexpr From(IT it) : _it(std::move(it)) { }

		template <class S>
		Range<IT, S> until(S s) && { return { std::move(_it), std::move(s) }; }
	};

	template <class P, class T>
	class Gen
	{
		P _gen;
		T _val;
	public:
		template <class S>
		constexpr Gen(P gen, S seed) : _gen(std::move(gen)), _val(std::invoke(_gen, seed)) { }

		using iterator_category = std::forward_iterator_tag;
		using difference_type = void;
		using value_type = remove_cvref_t<decltype(*_val)>;
		using reference = value_type & ;
		using pointer = value_type * ;

		Gen& operator++() { _val = std::invoke(_gen, _val); return *this; }

		constexpr reference operator*() const { return *_val; }
		constexpr pointer operator->() const { return _val; }

		constexpr bool operator==(End) const { return !static_cast<bool>(_val); }
		constexpr bool operator!=(End) const { return  static_cast<bool>(_val); }

		constexpr const Gen&  begin() const& { return *this; }
		constexpr       Gen&  begin() & { return *this; }
		constexpr       Gen&& begin() && { return *this; }
		constexpr End end() const { return {}; }
	};

	template <class P, class T>
	auto generator(P gen, T seed) -> Gen<P, remove_cvref_t<decltype(std::invoke(gen, seed))>>
	{
		return { std::move(gen), std::move(seed) };
	}

	template <class M>
	struct Map
	{
		M map;

		template<class IT, class S>
		class It : private Range<IT, S>
		{
			using Range<IT, S>::first;
			using Range<IT, S>::begin;
			using Range<IT, S>::end;
			M _map;
		public:
			constexpr It(M map, Range<IT, S> r) : Range<IT, S>(std::move(r)), _map(std::move(map)) { }

			constexpr It& operator++() { ++first; return *this; }

			constexpr decltype(auto) operator*() { return _map(*first); }
			constexpr const IT& operator->() { return first; }

			constexpr bool operator==(End) const { return begin() == end(); }
			constexpr bool operator!=(End) const { return begin() != end(); }
		};

		template <class C>
		constexpr auto of(C&& c) &&
		{
			return Range{ It{std::move(map), Range{ std::begin(c), std::end(c) } }, End{} };
		}
	};

	template <class IT, class S>
	class Split
	{
		Range<IT, S> _range;

		template <class P>
		class SuperIt;

		template <class P>
		class It
		{
			friend class Split;

			template <class V>
			decltype(auto) _check_done(V&& value)
			{
				if (_super->_pred(value))
					_super->_done = true;
				return std::forward<V>(value);
			}
			SuperIt<P>* _super;
		public:
			constexpr It(SuperIt<P>* super) : _super(super) { }

			constexpr It & operator++() { ++_super->_range.first; return *this; }

			constexpr decltype(auto) operator*() { return _check_done(*_super->_range.first); }

			constexpr bool operator==(End) const { return _super->_done || _super->_range.empty(); }
			constexpr bool operator!=(End) const { return !operator==({}); }
		};

		template <class P>
		class SuperIt
		{
			friend class It<P>;
			Range<IT, S> _range;
			P _pred;
			bool _done = false;
		public:
			constexpr SuperIt(Range<IT, S> r, P p) : _range(std::move(r)), _pred(std::move(p)) { }

			constexpr SuperIt& operator++() { _done = false; return *this; }

			constexpr Range<It<P>, End> operator*() { return { this, {} }; }

			constexpr bool operator==(End) const { return _range.empty(); }
			constexpr bool operator!=(End) const { return !operator==({}); }
		};
	public:
		constexpr Split(IT it, S end) : _range(std::move(it), std::move(end)) { }

		template <class P>
		Range<SuperIt<P>, End> after(P pred) && { return { { std::move(_range), std::move(pred)}, {} }; }
	};

	template <class C>
	constexpr auto split(C&& c) { return Split{ std::begin(c), std::end(c) }; }


	struct Those
	{
		template <class C>
		constexpr auto of(C&& c) const { return Range{ std::begin(c), std::end(c) }; }

		template <class IT>
		constexpr From<IT> from(IT it) const { return { std::move(it) }; }
	};
	static constexpr Those those;

	struct Each
	{
		template <class M>
		constexpr Map<M> operator()(M map) const { return { std::move(map) }; }

	};
	static constexpr Each each;

	struct First
	{
		template <class C>
		constexpr auto of(C&& c) const
		{
			using T = decltype(*std::begin(c));
			if constexpr (std::is_pointer_v<T>)
			{
				return std::empty(c) ?
					nullptr :
					*std::begin(c);
			}
			else if constexpr (std::is_reference_v<T>)
			{
				return std::empty(c) ?
					nullptr :
					std::addressof(*std::begin(c));
			}
			else
			{
				return std::empty(c) ?
					std::nullopt :
					std::make_optional(*std::begin(c));
			}
		}
	};
	static constexpr First first;



	struct Any
	{
		template <class IT, class S>
		struct Of
		{
			Range<IT, S> range;

			template <class Pred>
			constexpr bool are(Pred&& pred)
			{
				for (auto&& e : range)
					if (pred(e))
						return true;
				return false;
			}
		};


		template <class C>
		constexpr Of<BeginType<C>, EndType<C>> of(C&& c) const
		{ return { those.of(std::forward<C>(c)) }; }

	};
	static constexpr Any any;

	template <class P>
	auto isnt(P&& p) { return [pred = std::forward<P>(p)](auto&& value) { return !pred(value); }; }
}
