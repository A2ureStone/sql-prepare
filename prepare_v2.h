#include "include/binary_log_types.h"
#include "include/mysql.h"
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

struct MYSQL_NULL {};
// TODO improve cv
template <typename T> struct CppTypeToMysqlType {};

template <> struct CppTypeToMysqlType<signed char> {
  static constexpr enum_field_types value = MYSQL_TYPE_TINY;
};

template <> struct CppTypeToMysqlType<short int> {
  static constexpr enum_field_types value = MYSQL_TYPE_SHORT;
};

template <> struct CppTypeToMysqlType<int> {
  static constexpr enum_field_types value = MYSQL_TYPE_LONG;
};

template <> struct CppTypeToMysqlType<long long int> {
  static constexpr enum_field_types value = MYSQL_TYPE_LONGLONG;
};

template <> struct CppTypeToMysqlType<float> {
  static constexpr enum_field_types value = MYSQL_TYPE_FLOAT;
};

template <> struct CppTypeToMysqlType<double> {
  static constexpr enum_field_types value = MYSQL_TYPE_DOUBLE;
};

template <> struct CppTypeToMysqlType<MYSQL_NULL> {
  static constexpr enum_field_types value = MYSQL_TYPE_NULL;
};

// template <> struct CppTypeToMysqlType<MYSQL_TIME> {
//   static constexpr enum_field_types value = MYSQL_TYPE_DATETIME;
// };
//
// template <> struct CppTypeToMysqlType<char[]> {
//   static constexpr enum_field_types value = MYSQL_TYPE_STRING;
// };
template <> struct CppTypeToMysqlType<std::string> {
  static constexpr enum_field_types value = MYSQL_TYPE_STRING;
};
template <> struct CppTypeToMysqlType<const char *> {
  static constexpr enum_field_types value = MYSQL_TYPE_STRING;
};

constexpr size_t count_char(const char *str, char c) {
  if (str == nullptr) {
    return 0;
  }
  size_t cnt = 0;
  for (; *str != '\0'; ++str) {
    if (*str == c) {
      ++cnt;
    }
  }
  return cnt;
}

/* Derive Impl
   MYSQL_BIND* GetCurrentParam();
   std::pair<PARAM MYSQL_BIND*, size_t> GetParams();
   std::string GetStatement(); */
template <typename PrepareDerived> class PrepareBase {
public:
  PrepareBase(const std::string &statement) : statement(statement) {}
  std::pair<MYSQL_BIND *, size_t> GetParams() {
    // return {data.data(), data.size()};
    return static_cast<PrepareDerived *>(this)->GetParams();
  }
  std::string GetStatement() { return statement; }

private:
  // use type T to remove const and reference
  template <typename T>
  std::enable_if_t<std::is_floating_point_v<T>, void> AddParamImpl(T value) {
    MYSQL_BIND *data = static_cast<PrepareDerived *>(this)->GetCurrentParam();
    data->buffer_type = CppTypeToMysqlType<T>::value;
    data->buffer = (void *)&value;
  }

  template <typename T>
  std::enable_if_t<std::is_integral_v<T>, void> AddParamImpl(T value) {
    MYSQL_BIND *data = static_cast<PrepareDerived *>(this)->GetCurrentParam();
    data->buffer_type = CppTypeToMysqlType<T>::value;
    data->buffer = (void *)&value;
    data->is_unsigned = std::is_unsigned_v<T>;
  }

  void AddParamImpl(const char *str) {
    MYSQL_BIND *data = static_cast<PrepareDerived *>(this)->GetCurrentParam();
    data->buffer_type = MYSQL_TYPE_STRING;
    data->buffer = (void *)str;
    data->buffer_length = strlen(str);
  }

  void AddParamImpl(MYSQL_NULL value) {
    MYSQL_BIND *data = static_cast<PrepareDerived *>(this)->GetCurrentParam();
    data->buffer_type = CppTypeToMysqlType<MYSQL_NULL>::value;
    data->buffer = nullptr;
    data->length = nullptr;
  }

  void AddParamImpl(const std::string &value) {
    MYSQL_BIND *data = static_cast<PrepareDerived *>(this)->GetCurrentParam();
    data->buffer_type = CppTypeToMysqlType<std::string>::value;
    data->buffer = (void *)value.c_str();
    data->buffer_length = value.size();
  }

public:
  template <typename T> PrepareBase &AddParam(T value) {
    AddParamImpl(value);
    return *this;
  }
  PrepareBase &AddParam(const std::string &value) {
    AddParamImpl(value);
    return *this;
  }

  template <typename T> PrepareBase &operator()(T value) {
    return AddParam(value);
  }
  PrepareBase &operator()(const std::string &value) { return AddParam(value); }

protected:
  std::string statement;
};

template <size_t N> class StaticPrepare : public PrepareBase<StaticPrepare<N>> {
public:
  constexpr StaticPrepare(const std::string &statement)
      : PrepareBase<StaticPrepare<N>>(statement) {
    std::memset(data, 0, sizeof(data));
  }
  MYSQL_BIND *GetCurrentParam() {
    if (paramIndex >= N) {
      throw std::runtime_error("Prepare Statement Param Out of range");
    }
    return (data + paramIndex++);
  }
  std::pair<MYSQL_BIND *, size_t> GetParams() { return {data, N}; };

private:
  MYSQL_BIND data[N];
  size_t paramIndex{0};
};

// template <const char *STAT>
// using StaticPrepare = StaticPrepareImpl<count_char(STAT, '?')>;

class DynamicPrepare : public PrepareBase<DynamicPrepare> {
public:
  DynamicPrepare(std::string statement)
      : PrepareBase<DynamicPrepare>(statement) {
    data.resize(CountQuestionMark());
    std::memset(data.data(), 0, data.size() * sizeof(MYSQL_BIND));
  }

  MYSQL_BIND *GetCurrentParam() {
    if (paramIndex >= data.size()) {
      throw std::runtime_error("Prepare Statement Param Out of range");
    }
    return (data.data() + paramIndex++);
  }

  std::pair<MYSQL_BIND *, size_t> GetParams() {
    return {data.data(), data.size()};
  }

private:
  size_t CountQuestionMark() {
    size_t count = 0;
    for (auto c : statement) {
      if (c == '?') {
        count++;
      }
    }
    return count;
  }

  std::vector<MYSQL_BIND> data;
  size_t paramIndex{0};
};
