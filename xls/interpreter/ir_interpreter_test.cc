// Copyright 2020 The XLS Authors
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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "xls/common/status/matchers.h"
#include "xls/interpreter/function_interpreter.h"
#include "xls/interpreter/ir_evaluator_test_base.h"
#include "xls/ir/bits.h"
#include "xls/ir/bits_ops.h"
#include "xls/ir/function_builder.h"
#include "xls/ir/ir_parser.h"
#include "xls/ir/ir_test_base.h"
#include "xls/ir/package.h"
#include "xls/ir/verifier.h"

namespace xls {
namespace {

using status_testing::IsOkAndHolds;
using status_testing::StatusIs;
using testing::HasSubstr;

INSTANTIATE_TEST_SUITE_P(
    IrInterpreterTest, IrEvaluatorTestBase,
    testing::Values(IrEvaluatorTestParam(
        [](Function* function, absl::Span<const Value> args) {
          return InterpretFunction(function, args);
        },
        [](Function* function,
           const absl::flat_hash_map<std::string, Value>& kwargs) {
          return InterpretFunctionKwargs(function, kwargs);
        })));

// Fixture for IrInterpreter-only tests (i.e., those that aren't common to all
// IR evaluators).
class IrInterpreterOnlyTest : public IrTestBase {};

TEST_F(IrInterpreterOnlyTest, EvaluateNode) {
  Package package("my_package");
  std::string fn_text = R"(
    fn f(x: bits[4]) -> bits[4] {
      literal.1: bits[4] = literal(value=6)
      literal.2: bits[4] = literal(value=3)
      and.3: bits[4] = and(literal.1, x)
      ret or.4: bits[4] = or(literal.2, and.3)
    }
    )";

  XLS_ASSERT_OK_AND_ASSIGN(Function * function,
                           Parser::ParseFunction(fn_text, &package));

  EXPECT_THAT(InterpretNode(FindNode("and.3", function),
                            {Value(UBits(0b0011, 4)), Value(UBits(0b1010, 4))}),
              IsOkAndHolds(Value(UBits(0b0010, 4))));
  EXPECT_THAT(InterpretNode(FindNode("or.4", function),
                            {Value(UBits(0b0011, 4)), Value(UBits(0b1010, 4))}),
              IsOkAndHolds(Value(UBits(0b1011, 4))));
  EXPECT_THAT(InterpretNode(FindNode("literal.1", function), {}),
              IsOkAndHolds(Value(UBits(6, 4))));
}

TEST_F(IrInterpreterOnlyTest, SideEffectingNodes) {
  Package package("my_package");
  const std::string fn_text = R"(
    fn bar(tkn: token, cond: bits[1], x: bits[5]) -> bits[5] {
      trace.1: token = trace(tkn, cond, format="x is {}", data_operands=[x], id=1)
      umul.2 : bits[5] = umul(x, x, id=2)
      ret gate.3: bits[5] = gate(cond, umul.2, id=3)
    }
    )";

  XLS_ASSERT_OK_AND_ASSIGN(Function * function,
                           Parser::ParseFunction(fn_text, &package));

  EXPECT_THAT(
      InterpretNode(FindNode("trace.1", function),
                    {Value::Token(), Value::Bool(true), Value(UBits(17, 5))}),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Cannot interpret side-effecting op")));

  Node* gate_node = FindNode("gate.3", function);
  EXPECT_THAT(
      InterpretNode(gate_node, {Value::Bool(true), Value(UBits(17, 5))}),
      IsOkAndHolds(Value(UBits(0, 5))));
  EXPECT_THAT(
      InterpretNode(gate_node, {Value::Bool(false), Value(UBits(17, 5))}),
      IsOkAndHolds(Value(UBits(17, 5))));
}

}  // namespace
}  // namespace xls
