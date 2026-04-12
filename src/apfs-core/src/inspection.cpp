#include "orchard/apfs/inspection.h"

#include <utility>

#include "orchard/blockio/reader.h"

namespace orchard::apfs {
namespace {

InspectionResult MakeStatusOnly(const InspectionStatus status, std::string note) {
  InspectionResult result;
  result.status = status;
  result.notes.push_back(std::move(note));
  return result;
}

InspectionStatus MapErrorToStatus(const blockio::ErrorCode code) noexcept {
  switch (code) {
  case blockio::ErrorCode::kReadFailed:
  case blockio::ErrorCode::kShortRead:
  case blockio::ErrorCode::kOutOfRange:
  case blockio::ErrorCode::kIoctlFailed:
    return InspectionStatus::kReadFailed;
  case blockio::ErrorCode::kInvalidFormat:
  case blockio::ErrorCode::kCorruptData:
    return InspectionStatus::kParseFailed;
  case blockio::ErrorCode::kNotFound:
  case blockio::ErrorCode::kAccessDenied:
  case blockio::ErrorCode::kOpenFailed:
  case blockio::ErrorCode::kUnsupportedTarget:
  case blockio::ErrorCode::kInvalidArgument:
  case blockio::ErrorCode::kNotImplemented:
    return InspectionStatus::kOpenFailed;
  }

  return InspectionStatus::kOpenFailed;
}

} // namespace

InspectionResult InspectTarget(const blockio::InspectionTargetInfo& target_info) {
  switch (target_info.kind) {
  case blockio::TargetKind::kMissing:
    return MakeStatusOnly(InspectionStatus::kMissingTarget,
                          "Target path does not exist or could not be resolved.");
  case blockio::TargetKind::kDirectory:
    return MakeStatusOnly(InspectionStatus::kUnsupportedTarget,
                          "Directories are not valid APFS inspection targets.");
  case blockio::TargetKind::kUnknown:
    return MakeStatusOnly(InspectionStatus::kUnsupportedTarget,
                          "Target kind is not supported by the inspection path.");
  case blockio::TargetKind::kRegularFile:
  case blockio::TargetKind::kRawDevice:
    break;
  }

  InspectionResult result;

  auto reader_result = blockio::OpenReader(target_info);
  if (!reader_result.ok()) {
    result.status = InspectionStatus::kOpenFailed;
    result.error = reader_result.error();
    result.notes.push_back(reader_result.error().message);
    return result;
  }

  auto reader = std::move(reader_result).value();
  result.reader_backend = std::string(reader->backend_name());

  auto size_result = reader->size_bytes();
  if (size_result.ok()) {
    result.reader_size_bytes = size_result.value();
  } else {
    result.notes.push_back("Reader size is not available; discovery will use bounded scans.");
  }

  auto discovery_result = Discover(*reader);
  if (!discovery_result.ok()) {
    result.status = MapErrorToStatus(discovery_result.error().code);
    result.error = discovery_result.error();
    result.notes.push_back(discovery_result.error().message);
    return result;
  }

  result.report = std::move(discovery_result.value());
  result.notes.insert(result.notes.end(), result.report.notes.begin(), result.report.notes.end());
  result.status = result.report.containers.empty() ? InspectionStatus::kNoApfsContainer
                                                   : InspectionStatus::kSuccess;
  return result;
}

std::string_view ToString(const InspectionStatus status) noexcept {
  switch (status) {
  case InspectionStatus::kMissingTarget:
    return "missing_target";
  case InspectionStatus::kUnsupportedTarget:
    return "unsupported_target";
  case InspectionStatus::kOpenFailed:
    return "open_failed";
  case InspectionStatus::kReadFailed:
    return "read_failed";
  case InspectionStatus::kParseFailed:
    return "parse_failed";
  case InspectionStatus::kNoApfsContainer:
    return "no_apfs_container";
  case InspectionStatus::kSuccess:
    return "success";
  }

  return "unsupported_target";
}

} // namespace orchard::apfs
