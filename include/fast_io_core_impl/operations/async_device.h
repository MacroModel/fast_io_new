#pragma once

namespace fast_io::async
{
namespace details
{

/** @brief Owns an rvalue or stores the address of a borrowed lvalue across suspension. */
template <typename T>
class capture
{
	using value_type = ::std::remove_reference_t<T>;
	static inline constexpr bool borrowed{::std::is_lvalue_reference_v<T>};
	::std::conditional_t<borrowed, value_type *, value_type> storage;

	/** @brief Borrows a named object or constructs the single owned value. */
	static constexpr decltype(auto) initialize(T &&value) noexcept
	{
		if constexpr (borrowed)
		{
			return ::fast_io::freestanding::addressof(value);
		}
		else
		{
			return static_cast<T &&>(value);
		}
	}

public:
	/** @brief Records an operand using the value category supplied by the caller. */
	explicit constexpr capture(T &&value) noexcept(
		borrowed || ::std::is_nothrow_constructible_v<value_type, T &&>)
		: storage(initialize(static_cast<T &&>(value)))
	{}

	/** @brief Returns the stable named operand used by stream normalization. */
	constexpr value_type &get() noexcept
	{
		if constexpr (borrowed)
		{
			return *storage;
		}
		else
		{
			return storage;
		}
	}
	/** @brief Preserves a borrowed stream's mutability but keeps a const owned stream const. */
	constexpr decltype(auto) get() const noexcept
	{
		if constexpr (borrowed)
		{
			return *storage;
		}
		else
		{
			return (storage);
		}
	}
};

/** @brief Proves the operation can keep the operand alive in its coroutine frame. */
template <typename T>
concept capturable = ::std::is_lvalue_reference_v<T> ||
					 ::std::constructible_from<::std::remove_reference_t<T>, T &&>;

/** @brief Marks a captured endpoint for direction-specific asynchronous dispatch. */
template <typename T, typename Path = void>
struct device : capture<T>
{
	using capture<T>::capture;
	using stream_category = T;
	using layer_path = Path;
};

template <typename T>
inline constexpr bool is_device = false;

template <typename T, typename Path>
inline constexpr bool is_device<device<T, Path>> = true;

} // namespace details

/** @brief Owns or borrows one operand and selects its provider-defined asynchronous observations. */
template <typename Selection,typename T> requires details::capturable<T>
struct basic_async : details::capture<T>
{
	using details::capture<T>::capture;
	using stream_category=T;
	using selection_type=Selection;
	using operand_type=T;
};
namespace details
{
template <typename> inline constexpr bool is_annotation = false;
template <typename S, typename T> inline constexpr bool is_annotation<basic_async<S,T>> = true;
}

/** @brief Recognizes only annotations constructed for the public per-operand interface. */
template <typename T>
concept annotated_operand = details::is_annotation<::std::remove_cvref_t<T>>;

/** @brief Borrows the annotated endpoint's input projection for a decorator's ordinary stream protocol.
 * @details The decorator retains the original operand, including its selection and ownership. The
 * projection only supplies the character domain and ordinary primitives of this particular direction.
 */
template <annotated_operand T>
	requires requires(T &value) { ::fast_io::operations::input_stream_ref(value.get()); }
constexpr decltype(auto) input_stream_ref_define(T &&value)
	noexcept(noexcept(::fast_io::operations::input_stream_ref(value.get())))
{
	return ::fast_io::operations::input_stream_ref(value.get());
}

/** @brief Projects an annotated writable endpoint without starting an operation or moving its owner. */
template <annotated_operand T>
	requires requires(T &value) { ::fast_io::operations::output_stream_ref(value.get()); }
constexpr decltype(auto) output_stream_ref_define(T &&value)
	noexcept(noexcept(::fast_io::operations::output_stream_ref(value.get())))
{
	return ::fast_io::operations::output_stream_ref(value.get());
}

namespace details
{
/** @brief Finds an explicit marker in a stored decorator expression without normalizing a value or opening a file. */
template <typename T>
consteval bool contains_annotation() noexcept
{
	using operand = ::std::remove_cvref_t<T>;
	if constexpr (annotated_operand<operand>)
		return true;
	else if constexpr (requires { typename operand::decorated_stream_type; })
		return contains_annotation<typename operand::decorated_stream_type>();
	else if constexpr (requires { typename operand::buffered_stream_type; })
		return contains_annotation<typename operand::buffered_stream_type>();
	else
		return false;
}
} // namespace details

namespace details
{
/** @brief Takes a value annotation apart in its original category. A stored lvalue annotation continues to borrow its owner. */
template <typename T>
constexpr decltype(auto) value_operand(T &&v) noexcept
{
	if constexpr (annotated_operand<T>)
	{
		using source=typename ::std::remove_cvref_t<T>::operand_type;
		if constexpr (!::std::is_lvalue_reference_v<T> && !::std::is_lvalue_reference_v<source>)
			// Moving a const owning annotation can only offer a const rvalue to the provider.
			return static_cast<::std::remove_reference_t<decltype(v.get())> &&>(v.get());
		else return v.get();
	}
	else return static_cast<T &&>(v);
}
/** @brief Recognizes a direction selected privately by the operation binder. */
template <typename T>
concept marked_device = is_device<::std::remove_cvref_t<T>> || annotated_operand<T>;
} // namespace details
} // namespace fast_io::async
