#pragma once

#include "Platform.hpp"
#if PLATFORM_ARCH != PLATFORM_ARCHITECTURE_AVR
#  include <type_traits>
#else
namespace std {

template <class _Ty, _Ty _Val> struct integral_constant {
  static constexpr _Ty value = _Val;

  using value_type = _Ty;
  using type = integral_constant;

  constexpr operator value_type() const noexcept { return value; }

  constexpr value_type operator()() const noexcept { return value; }
};

template <bool _Val> using bool_constant = integral_constant<bool, _Val>;

using true_type = bool_constant<true>;
using false_type = bool_constant<false>;

template <class, class> inline constexpr bool is_same_v = false;
template <class _Ty> inline constexpr bool is_same_v<_Ty, _Ty> = true;

template <class _Ty1, class _Ty2>
struct is_same : bool_constant<is_same_v<_Ty1, _Ty2>> {};

} // namespace std
#endif

#include <IPAddress.h>

namespace net {

template <typename T> struct has_remoteIP {
  template <typename U> static constexpr std::false_type test(...) {
    return {};
  }
  template <typename U>
  static constexpr auto test(U *u) ->
      typename std::is_same<IPAddress, decltype(u->remoteIP())>::type {
    return {};
  }

  static constexpr bool value{test<T>(nullptr)};
};

} // namespace net
