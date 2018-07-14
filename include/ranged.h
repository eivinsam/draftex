#pragma once

#include <type_traits>
#include <optional>

namespace ranged
{
	template <class T>
	using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

	struct End {};

	template <class IT, class S>
	struct range
	{
		IT first;
		S last;

		constexpr range(IT first, S last) : first(std::move(first)), last(std::move(last)) { }
		template <class C>
		constexpr range(C&& c) : first(std::begin(c)), last(std::end(c)) { }

		constexpr IT begin() const { return first; }
		constexpr S end() const { return last; }

		constexpr bool empty() const { return first == last; }
	};
	template <class IT>
	struct range<IT, End>
	{
		IT first;

		constexpr range(IT first, End) : first(std::move(first)) { }
		template <class C>
		constexpr range(C&& c) : first(std::begin(c))
		{ 
			static_assert(std::is_same_v<End, remove_cvref_t<decltype(std::end(c))>>);
		}

		constexpr IT begin() const { return first; }
		constexpr End end() const { return {}; }

		constexpr bool empty() const { return first == End{}; }
	};


	template <class C> using BeginType = remove_cvref_t<decltype(std::begin(std::declval<C>()))>;
	template <class C> using   EndType = remove_cvref_t<decltype(std::end  (std::declval<C>()))>;

	template <class C>
	range(C&&) -> range<BeginType<C>, EndType<C>>;

	template <class C>
	using RangeType = decltype(range(std::declval<C>()));

	template <class Arg, class Op, class Result = decltype(std::declval<Op>()(std::declval<Arg>()))>
	constexpr Result operator|(Arg&& arg, Op&& op) { return (std::forward<Op>(op))(std::forward<Arg>(arg)); }

	template <class P, class T>
	class generator
	{
		P _gen;
		T _val;
	public:
		generator(T seed, P gen) : _gen(std::move(gen)), _val(std::invoke(_gen, seed)) { }

		using iterator_category = std::forward_iterator_tag;
		using difference_type = void;
		using value_type = std::decay_t<decltype(*std::declval<T>())>;
		using reference = value_type & ;
		using pointer = value_type * ;

		generator& operator++() { _val = std::invoke(_gen, _val); return *this; }

		constexpr reference operator*() const { return *_val; }
		constexpr pointer operator->() const { return _val; }

		constexpr bool operator==(End) const { return !static_cast<bool>(_val); }
		constexpr bool operator!=(End) const { return  static_cast<bool>(_val); }

		constexpr const generator&  begin() const& { return *this; }
		constexpr       generator&  begin() &  { return *this; }
		constexpr       generator&& begin() && { return *this; }
		constexpr End end() const { return {}; }
	};

	template <class Pred>
	struct filter
	{
		Pred p;

		constexpr filter(Pred p) : p(std::move(p)) { }

		template <class IT, class S>
		class iterator
		{
			Pred _p;
			range<IT, S> _range;

			void _correct_p()
			{
				while (!_range.empty() && !_p(*_range.first))
					++_range.first;
			}
		public:
			iterator(Pred p, IT it, S end) : _p(std::move(p)), _range(std::move(it), std::move(end)) 
			{ 
				_correct_p();
			}

			iterator& operator++()
			{ 
				++_range.first;
				_correct_p();
				return *this; 
			}

			auto&& operator*() { return *_range.first; }
			auto&& operator->() { return _range.first; }

			constexpr bool operator==(End) const { return  _range.empty(); }
			constexpr bool operator!=(End) const { return !_range.empty(); }
		};

		template <class C, class IT = BeginType<C>, class S = EndType<C>>
		range<iterator<IT, S>, End> operator()(C&& c) const
		{
			return { { p, std::begin(c), std::end(c) }, {} };
		}
	};

	template <class C>
	constexpr auto first(C&& c)
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
}
