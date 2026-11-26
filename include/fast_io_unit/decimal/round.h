#pragma once

namespace fast_io::details
{

using decimal_rounding = ::fast_io::manipulators::floating_rounding;

inline constexpr decimal_rounding decimal_effective_rounding(decimal_rounding mode) noexcept
{
	return mode == decimal_rounding::current_environment ? current_floating_rounding() : mode;
}

/// guard is the first discarded digit and sticky represents every following digit.
/// Retained parity is decimal digit parity. Sign enters only the directed policies.
inline constexpr bool decimal_increment(decimal_rounding mode, bool negative, unsigned guard, bool sticky, bool odd) noexcept
{
	if (!guard && !sticky)
	{
		return false;
	}
	switch (decimal_effective_rounding(mode))
	{
	case decimal_rounding::toward_zero:
		return false;
	case decimal_rounding::away_from_zero:
		return true;
	case decimal_rounding::toward_plus_infinity:
		return !negative;
	case decimal_rounding::toward_minus_infinity:
		return negative;
	default:
		break;
	}
	if (guard != 5u || sticky)
	{
		return 5u <= guard;
	}
	switch (mode)
	{
	case decimal_rounding::nearest_to_odd:
		return !odd;
	case decimal_rounding::nearest_toward_plus_infinity:
		return !negative;
	case decimal_rounding::nearest_toward_minus_infinity:
		return negative;
	case decimal_rounding::nearest_toward_zero:
		return false;
	case decimal_rounding::nearest_away_from_zero:
		return true;
	default:
		return odd;
	}
}

template <typename Decimal>
inline constexpr void decimal_round(Decimal &value, int keep, bool sticky, decimal_rounding mode,
									bool keep_quantum_on_carry = false) noexcept
{
	auto const size{static_cast<int>(value.size)};
	if (size <= keep)
	{
		return;
	}
	unsigned guard{};
	if (keep < 0)
	{
		sticky = sticky || value.digits[0] != 0;
	}
	else
	{
		guard = value.digits[keep];
		for (auto i{keep + 1}; i < size; ++i)
		{
			sticky |= value.digits[i] != 0u;
		}
	}
	auto carry{decimal_increment(mode, value.negative, guard, sticky,
								 0 < keep && (value.digits[keep - 1] & 1u))};
	value.exponent += size - keep;
	if (keep <= 0)
	{
		value.size = 1u;
		value.digits[0] = static_cast<unsigned char>(carry);
		return;
	}
	value.size = static_cast<::std::size_t>(keep);
	if (!carry) return;
	// Carry propagates only through the retained digits. Naming the
	// successful non-nine case directly also leaves the exact lower bound
	// of the reverse traversal visible to optimized GCC array diagnostics.
	for (unsigned index{static_cast<unsigned>(keep)}; index != 0u; )
	{
		--index;
		auto digit{value.digits[index]};
		if (digit < 9u)
		{
			value.digits[index] = static_cast<unsigned char>(digit + 1u);
			return;
		}
		value.digits[index] = 0u;
	}
	value.digits[0] = 1u;
	if (keep_quantum_on_carry)
	{
		value.digits[value.size++] = 0u;
	}
	else
	{
		++value.exponent;
	}
}

} // namespace fast_io::details
