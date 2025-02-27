// Copyright 2024 The XLS Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "xls/public/c_api.h"

#include <string>

#include "absl/cleanup/cleanup.h"
#include "gtest/gtest.h"
#include "xls/common/file/filesystem.h"
#include "xls/common/file/temp_directory.h"
#include "xls/common/status/matchers.h"
#include "xls/dslx/default_dslx_stdlib_path.h"

namespace {

using testing::HasSubstr;

// Smoke test for `xls_convert_dslx_to_ir` C API.
TEST(XlsCApiTest, ConvertDslxToIrSimple) {
  const std::string kProgram = "fn id(x: u32) -> u32 { x }";
  const char* additional_search_paths[] = {};
  char* error_out = nullptr;
  char* ir_out = nullptr;
  bool ok =
      xls_convert_dslx_to_ir(kProgram.c_str(), "my_module.x", "my_module",
                             /*dslx_stdlib_path=*/xls::kDefaultDslxStdlibPath,
                             additional_search_paths, 0, &error_out, &ir_out);

  absl::Cleanup free_cstrs([&] {
    free(error_out);
    free(ir_out);
  });

  // We should get IR and no error.
  ASSERT_TRUE(ok);
  ASSERT_EQ(error_out, nullptr);
  ASSERT_NE(ir_out, nullptr);

  EXPECT_THAT(ir_out, HasSubstr("fn __my_module__id"));
}

TEST(XlsCApiTest, ConvertDslxToIrError) {
  const std::string kInvalidProgram = "@!";
  const char* additional_search_paths[] = {};
  char* error_out = nullptr;
  char* ir_out = nullptr;

  absl::Cleanup free_cstrs([&] {
    free(error_out);
    free(ir_out);
  });

  bool ok = xls_convert_dslx_to_ir(
      kInvalidProgram.c_str(), "my_module.x", "my_module",
      /*dslx_stdlib_path=*/xls::kDefaultDslxStdlibPath, additional_search_paths,
      0, &error_out, &ir_out);
  ASSERT_FALSE(ok);

  // We should get an error and not get IR.
  ASSERT_NE(error_out, nullptr);
  ASSERT_EQ(ir_out, nullptr);

  EXPECT_THAT(error_out, HasSubstr("Unrecognized character: '@'"));
}

// Smoke test for `xls_convert_dslx_path_to_ir` C API.
TEST(XlsCApiTest, ConvertDslxPathToIr) {
  const std::string kProgram = "fn id(x: u32) -> u32 { x }";

  XLS_ASSERT_OK_AND_ASSIGN(xls::TempDirectory tempdir,
                           xls::TempDirectory::Create());
  const std::filesystem::path module_path = tempdir.path() / "my_module.x";
  XLS_ASSERT_OK(xls::SetFileContents(module_path, kProgram));

  const char* additional_search_paths[] = {};
  char* error_out = nullptr;
  char* ir_out = nullptr;
  bool ok = xls_convert_dslx_path_to_ir(
      module_path.c_str(), /*dslx_stdlib_path=*/xls::kDefaultDslxStdlibPath,
      additional_search_paths, 0, &error_out, &ir_out);

  absl::Cleanup free_cstrs([&] {
    free(error_out);
    free(ir_out);
  });

  // We should get IR and no error.
  ASSERT_TRUE(ok);
  ASSERT_EQ(error_out, nullptr);
  ASSERT_NE(ir_out, nullptr);

  EXPECT_THAT(ir_out, HasSubstr("fn __my_module__id"));
}

TEST(XlsCApiTest, ParseTypedValueAndFreeIt) {
  char* error = nullptr;
  struct xls_value* value = nullptr;
  ASSERT_TRUE(xls_parse_typed_value("bits[32]:0x42", &error, &value));

  char* string_out = nullptr;
  ASSERT_TRUE(xls_value_to_string(value, &string_out));
  EXPECT_EQ(std::string{string_out}, "bits[32]:66");
  free(string_out);

  xls_value_free(value);
}

TEST(XlsCApiTest, ParsePackageAndInterpretFunctionInIt) {
  const std::string kPackage = R"(
package p

fn f(x: bits[32]) -> bits[32] {
  ret y: bits[32] = identity(x)
}
)";

  char* error = nullptr;
  struct xls_package* package = nullptr;
  ASSERT_TRUE(xls_parse_ir_package(kPackage.c_str(), "p.ir", &error, &package))
      << "xls_parse_ir_package error: " << error;
  absl::Cleanup free_package([package] { xls_package_free(package); });

  struct xls_function* function = nullptr;
  ASSERT_TRUE(xls_package_get_function(package, "f", &error, &function));

  struct xls_value* ft = nullptr;
  ASSERT_TRUE(xls_parse_typed_value("bits[32]:0x42", &error, &ft));
  absl::Cleanup free_ft([ft] { xls_value_free(ft); });

  const struct xls_value* args[] = {ft};

  struct xls_value* result = nullptr;
  ASSERT_TRUE(
      xls_interpret_function(function, /*argc=*/1, args, &error, &result));
  absl::Cleanup free_result([result] { xls_value_free(result); });

  ASSERT_TRUE(xls_value_eq(ft, result));
}

}  // namespace
