#include "prepare_v2.h"
#include <iostream>
#include <type_traits>

template <typename T> int func(T t) { return 0; }

template <> int func(int t) { return 1; }

int main() {
  static constexpr const char *str =
      "select * from table where a = ? and b = ? and c = ?";
  // StaticPrepareImpl<count_char(str, '?')> prepare("abc");
  // StaticPrepare<count_char(str, '?')> prepare("abc");
  StaticPrepare<count_char(str, '?')> prepare("abc");
  std::string ab = "abc";
  prepare.AddParam(ab);
  std::cout << prepare.GetParams().first[0].buffer_type << std::endl;
  std::remove_cv_t<const char *> a;
  std::is_pointer<char *> tr;
  using charptr = std::remove_cv_t<const volatile char *const volatile>;
  static_assert(std::is_same_v<charptr, char *>);
}
