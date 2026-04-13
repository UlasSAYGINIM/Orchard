#include "orchard/fs_winfsp/path_bridge.h"

#include <Windows.h>

#include <algorithm>
#include <vector>

#include "orchard/apfs/format.h"

namespace orchard::fs_winfsp {
namespace {

constexpr wchar_t kBackslash = L'\\';
constexpr wchar_t kSlash = L'/';

bool EqualsWideChar(const wchar_t left, const wchar_t right, const bool case_insensitive) noexcept {
  return ::CompareStringOrdinal(&left, 1, &right, 1, case_insensitive ? TRUE : FALSE) == CSTR_EQUAL;
}

bool IsInvalidPathByte(const unsigned char value) noexcept {
  if (value < 0x20U) {
    return true;
  }

  switch (value) {
  case '"':
  case '<':
  case '>':
  case '|':
  case '?':
  case '*':
    return true;
  default:
    return false;
  }
}

} // namespace

blockio::Result<std::string> WideToUtf8(const std::wstring_view text) {
  if (text.empty()) {
    return std::string{};
  }

  const auto size =
      ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) {
    return orchard::apfs::MakeApfsError(blockio::ErrorCode::kInvalidArgument,
                                        "Failed to convert UTF-16 text to UTF-8.");
  }

  std::string utf8(static_cast<std::size_t>(size), '\0');
  if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), utf8.data(), size, nullptr,
                            nullptr) != size) {
    return orchard::apfs::MakeApfsError(blockio::ErrorCode::kInvalidArgument,
                                        "Failed to convert UTF-16 text to UTF-8.");
  }

  return utf8;
}

blockio::Result<std::wstring> Utf8ToWide(const std::string_view text) {
  if (text.empty()) {
    return std::wstring{};
  }

  const auto size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                          static_cast<int>(text.size()), nullptr, 0);
  if (size <= 0) {
    return orchard::apfs::MakeApfsError(blockio::ErrorCode::kInvalidArgument,
                                        "Failed to convert UTF-8 text to UTF-16.");
  }

  std::wstring wide(static_cast<std::size_t>(size), L'\0');
  if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), wide.data(), size) != size) {
    return orchard::apfs::MakeApfsError(blockio::ErrorCode::kInvalidArgument,
                                        "Failed to convert UTF-8 text to UTF-16.");
  }

  return wide;
}

blockio::Result<std::string> NormalizeWindowsPath(const std::wstring_view windows_path) {
  if (windows_path.empty() || windows_path == L"\\" || windows_path == L"/") {
    return std::string("/");
  }

  auto utf8_result = WideToUtf8(windows_path);
  if (!utf8_result.ok()) {
    return utf8_result.error();
  }

  std::vector<std::string> components;
  const auto& utf8 = utf8_result.value();
  std::size_t current = 0U;
  while (current < utf8.size()) {
    while (current < utf8.size() && (utf8[current] == '\\' || utf8[current] == '/')) {
      ++current;
    }
    if (current >= utf8.size()) {
      break;
    }

    auto next = utf8.find_first_of("\\/", current);
    if (next == std::string::npos) {
      next = utf8.size();
    }

    const auto component = utf8.substr(current, next - current);
    for (const auto ch : component) {
      if (ch == ':') {
        return orchard::apfs::MakeApfsError(
            blockio::ErrorCode::kNotImplemented,
            "WinFsp stream syntax is not supported by the Orchard M2 read-only adapter.");
      }
      if (IsInvalidPathByte(static_cast<unsigned char>(ch))) {
        return orchard::apfs::MakeApfsError(blockio::ErrorCode::kInvalidArgument,
                                            "The Windows path contains unsupported characters.");
      }
    }

    if (component == ".") {
      current = next;
      continue;
    }
    if (component == "..") {
      if (!components.empty()) {
        components.pop_back();
      }
      current = next;
      continue;
    }

    components.push_back(component);
    current = next;
  }

  if (components.empty()) {
    return std::string("/");
  }

  std::string normalized;
  for (const auto& component : components) {
    normalized.push_back('/');
    normalized += component;
  }

  return normalized;
}

blockio::Result<std::wstring> OrchardPathToWindowsPath(const std::string_view orchard_path) {
  auto wide_result = Utf8ToWide(orchard_path);
  if (!wide_result.ok()) {
    return wide_result.error();
  }

  auto windows_path = std::move(wide_result.value());
  if (windows_path.empty() || windows_path == L"/") {
    return std::wstring(L"\\");
  }

  std::replace(windows_path.begin(), windows_path.end(), kSlash, kBackslash);
  if (!windows_path.empty() && windows_path.front() != kBackslash) {
    windows_path.insert(windows_path.begin(), kBackslash);
  }

  return windows_path;
}

int CompareDirectoryNames(const std::wstring_view left, const std::wstring_view right,
                          const bool case_insensitive) noexcept {
  const auto result =
      ::CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                             static_cast<int>(right.size()), case_insensitive ? TRUE : FALSE);
  switch (result) {
  case CSTR_LESS_THAN:
    return -1;
  case CSTR_EQUAL:
    return 0;
  case CSTR_GREATER_THAN:
    return 1;
  default:
    return 0;
  }
}

bool MatchesDirectoryPattern(const std::wstring_view name, const std::wstring_view pattern,
                             const bool case_insensitive) noexcept {
  if (pattern.empty() || pattern == L"*" || pattern == L"*.*") {
    return true;
  }

  std::size_t name_index = 0U;
  std::size_t pattern_index = 0U;
  std::size_t star_pattern_index = std::wstring_view::npos;
  std::size_t star_name_index = 0U;

  while (name_index < name.size()) {
    if (pattern_index < pattern.size() && pattern[pattern_index] == L'*') {
      star_pattern_index = pattern_index++;
      star_name_index = name_index;
      continue;
    }

    if (pattern_index < pattern.size() &&
        (pattern[pattern_index] == L'?' ||
         EqualsWideChar(name[name_index], pattern[pattern_index], case_insensitive))) {
      ++name_index;
      ++pattern_index;
      continue;
    }

    if (star_pattern_index != std::wstring_view::npos) {
      pattern_index = star_pattern_index + 1U;
      name_index = ++star_name_index;
      continue;
    }

    return false;
  }

  while (pattern_index < pattern.size() && pattern[pattern_index] == L'*') {
    ++pattern_index;
  }

  return pattern_index == pattern.size();
}

} // namespace orchard::fs_winfsp
