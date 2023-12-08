//
// Created by Yihua on 2023/11/28.
//

#include <algorithm>
#include <tuple>
#include <vector>
#include "Approx.h"

template<typename U, std::floating_point T>
ApproxBase<U, T>::ApproxBase(U expected, const T abs, const T rel, const bool nan_ok) : expected(expected), abs(abs), rel(rel), nan_ok(nan_ok) {}

template<typename U, std::floating_point T>
ApproxBase<U, T>::ApproxBase(const ApproxBase &other) : expected(other.expected), abs(other.abs), rel(other.rel), nan_ok(other.nan_ok) {}

template<typename U, std::floating_point T>
ApproxBase<U, T>::ApproxBase(ApproxBase &&other) noexcept : expected(std::move(other.expected)), abs(other.abs), rel(other.rel), nan_ok(other.nan_ok) {}

template<typename U, std::floating_point T>
auto ApproxBase<U, T>::operator=(const ApproxBase &other) -> ApproxBase<U, T>& {
    expected = other.expected;
    abs = other.abs;
    rel = other.rel;
    nan_ok = other.nan_ok;
    return *this;
}

template<typename U, std::floating_point T>
auto ApproxBase<U, T>::operator=(ApproxBase &&other) noexcept -> ApproxBase<U, T> & {
    expected = std::move(other.expected);
    abs = other.abs;
    rel = other.rel;
    nan_ok = other.nan_ok;
    return *this;
}

template<typename U, std::floating_point T>
// template<typename V>
// requires std::ranges::sized_range<V>
auto ApproxBase<U, T>::operator==(const U &actual) const -> bool {
    if constexpr (std::ranges::sized_range<U>) {
        // if constexpr (std::same_as<std::ranges::range_value_t<U>, T>) {
        // For issue "In template: type constraint differs in template redeclaration",
        // see https://youtrack.jetbrains.com/issue/CPP-35055/CLion-false-error-on-using-C23-zip-feature.
        return std::ranges::all_of(
            // std::any_cast<decltype(std::views::zip(expected, actual))>(_yield_comparisons(actual)),
            std::views::zip(expected, actual),
            [this](const std::tuple<T, T> &elem) -> bool {
                T a = std::get<0>(elem);
                T x = std::get<1>(elem);
                // Since C++20, given operator==, != implies !(==).
                return a == _approx_scalar(x);
            });
    }
    throw std::logic_error("The operator==() function of the base class ApproxBase<U, T> should only be called when"
                           "U is a container.");
}

/*
 * Yield all the pairs of numbers to be compared.
 * This is used to implement the `operator==` method.
 */
template<typename U, std::floating_point T>
auto ApproxBase<U, T>::_yield_comparisons(const U &actual) const -> std::any {
    throw std::logic_error("Not Implemented.");
}

template<typename U, std::floating_point T>
template<typename V, typename>
auto ApproxBase<U, T>::_approx_scalar(T x) const -> ApproxScalar<U, T> {
    return ApproxScalar<U, T>(x, abs, rel, nan_ok);
}

template<std::ranges::sized_range U, std::floating_point T>
auto ApproxSequenceLike<U, T>::operator==(const U &actual) const -> bool {
    return (std::ranges::size(actual) == std::ranges::size(this->expected)) and ApproxBase<U, T>::operator==(actual);
}

template<std::ranges::sized_range U, std::floating_point T>
auto ApproxSequenceLike<U, T>::_yield_comparisons(const U &actual) const -> std::any {
    return std::views::zip(actual, this->expected);
}

/*
 * Return whether the given value is equal to the expected value
 * within the pre-specified tolerance.
 */
template<FPScalar U, std::floating_point T>
auto ApproxScalar<U, T>::operator==(const U &actual) const -> bool {
    // Short-circuit exact equality.
    if (actual == this->expected) {
        return true;
    }
    // If either type is non-numeric, fall back to strict equality.
    if constexpr (not std::is_floating_point_v<T> or not is_fp_complex<T>::value) {
        return false;
    }
    // Allow the user to control whether NaNs are considered equal to each or not.
    // The abs() calls are for compatibility with complex numbers.
    if (std::isnan(std::abs(this->expected))) {
        return this->nan_ok and std::isnan(std::abs(actual));
    }
    // Infinity shouldn't be approximately equal to anything but itself, but
    // if there's a relative tolerance, it will be infinite and infinity
    // will seem approximately equal to everything.  The equal-to-itself
    // case would have been short-circuited above, so here we can just
    // return false if the expected value is infinite.  The abs() call is
    // for compatibility with complex numbers.
    if (std::isinf(std::abs(this->expected))) {
        return false;
    }
    // Return true if the two numbers are within the tolerance.
    return std::abs(this->expected - actual) <= this->tolerance();
}

/*
 * Return the tolerance for the comparison.
 *
 * This could be either an absolute tolerance or a relative tolerance,
 * depending on what the user specified or which would be larger.
 */
template<FPScalar U, std::floating_point T>
auto ApproxScalar<U, T>::tolerance() const -> T {
    auto set_default = [](const T x, const T _default) -> T {
        return (not std::isnan(x)) ? x : _default;
    };
    // Figure out what the absolute tolerance should be.
    T absolute_tolerance = set_default(this->abs, DEFAULT_ABSOLUTE_TOLERANCE);

    if (absolute_tolerance < 0) {
        throw std::runtime_error("Absolute tolerance cannot be negative: " + std::to_string(absolute_tolerance));
    }
    if (std::isnan(absolute_tolerance)) {
        throw std::runtime_error("Absolute tolerance cannot be NaN.");
    }
    // If the user specified an absolute tolerance but not a relative one,
    // just return the absolute tolerance.
    if (std::isnan(this->rel) and not std::isnan(this->abs)) {
        return absolute_tolerance;
    }
    // Figure out what the relative tolerance should be.
    T relative_tolerance = set_default(this->rel, DEFAULT_RELATIVE_TOLERANCE) * std::abs(this->expected);
    if (relative_tolerance < 0) {
        throw std::runtime_error("Relative tolerance cannot be negative: " + std::to_string(relative_tolerance));
    }
    if (std::isnan(relative_tolerance)) {
        throw std::runtime_error("Relative tolerance cannot be NaN.");
    }
    // Return the largest of the relative and absolute tolerances.
    return std::max(relative_tolerance, absolute_tolerance);
}

template<std::ranges::sized_range U, std::floating_point T>
auto approx(const U &expected, const T rel, const T abs, const bool nan_ok) -> ApproxSequenceLike<U, T> {
    return ApproxSequenceLike<U, T>(expected, rel, abs, nan_ok);
}

template<FPScalar U, std::floating_point T>
auto approx(const U expected, const T rel, const T abs, const bool nan_ok) -> ApproxScalar<U, T> {
    return ApproxScalar<U, T>(expected, rel, abs, nan_ok);
}

template auto approx(const std::vector<double> &expected, const double rel, const double abs, const bool nan_ok) -> ApproxSequenceLike<std::vector<double>, double>;
template auto approx(const double expected, const double rel, const double abs, const bool nan_ok) -> ApproxScalar<double, double>;
template auto approx(const std::complex<double> expected, const double rel, const double abs, const bool nan_ok) -> ApproxScalar<std::complex<double>, double>;
