#include "support/FakeClock.h"
#include <cmath>
#include <cstring>
TEST_CASE("2.4 unknown operation is a load error naming the node pointer") {
    RuleFixture f;
    f.nodes[0].operation = static_cast<Operation>(255);
    auto status = f.rule.Load(f.definition, f.signals);
    CHECK(status.code == ErrorCode::unknown_operation);
    CHECK(std::strstr(status.message, "/root") != nullptr);
}
TEST_CASE("2.4 depth 33 is rejected with the node pointer") {
    RuleFixture f;
    std::array<ExpressionNode, 33> nodes{};
    for (std::uint16_t i = 0; i < 32; ++i)
        nodes[i] = {
            Operation::absolute_value, 0, Unit::dimensionless, 0, static_cast<std::uint16_t>(i + 1), 1, "/deep"};
    nodes[32] = {Operation::literal, 1, Unit::dimensionless, 0, 0, 0, "/depth33"};
    f.definition.nodes = nodes;
    auto status = f.rule.Load(f.definition, f.signals);
    CHECK(status.code == ErrorCode::expression_depth);
    CHECK(std::strstr(status.message, "/depth33") != nullptr);
}
TEST_CASE("2.4 node count 513 is rejected with the node pointer") {
    RuleFixture f;
    std::array<ExpressionNode, 513> nodes{};
    nodes[512].source_pointer = "/node513";
    f.definition.nodes = nodes;
    auto status = f.rule.Load(f.definition, f.signals);
    CHECK(status.code == ErrorCode::expression_nodes);
    CHECK(std::strstr(status.message, "/node513") != nullptr);
}
TEST_CASE("2.4 pressure literal against temperature signal is a load error naming both units") {
    RuleFixture f;
    f.nodes[2].literal_unit = Unit::pascal;
    auto status = f.rule.Load(f.definition, f.signals);
    CHECK(status.code == ErrorCode::incompatible_units);
    CHECK(std::strstr(status.message, "Pa") != nullptr);
    CHECK(std::strstr(status.message, "K") != nullptr);
}
TEST_CASE("2.4 degC threshold evaluates at the correct kelvin point") {
    RuleFixture f;
    f.Load();
    f.Apply(373.14);
    CHECK_FALSE(f.Evaluate().current);
    f.Apply(373.16, 6ms, 2);
    CHECK(f.Evaluate().current);
}
TEST_CASE("2.4 division by zero yields invalid") {
    RuleFixture f;
    f.nodes[0].operation = Operation::divide;
    f.nodes[1] = {Operation::literal, 1, Unit::dimensionless};
    f.nodes[2] = {Operation::literal, 0, Unit::dimensionless};
    f.Load();
    auto result = f.Evaluate();
    CHECK(result.quality == Quality::invalid);
    CHECK(std::isnan(result.value));
}
TEST_CASE("2.4 overflowing product yields invalid") {
    RuleFixture f;
    f.nodes[0].operation = Operation::multiply;
    f.nodes[1] = {Operation::literal, 1e308, Unit::dimensionless};
    f.nodes[2] = f.nodes[1];
    f.Load();
    CHECK(f.Evaluate().quality == Quality::invalid);
}
TEST_CASE("2.4 stale input routes to missing input policy") {
    RuleFixture f;
    f.definition.missing_input_policy = MissingInputPolicy::force_warn;
    f.Load();
    f.Apply(1);
    f.Storage()[0].sample.quality = Quality::stale;
    auto result = f.Evaluate();
    CHECK(result.current);
    CHECK(result.quality == Quality::valid);
}
TEST_CASE("2.4 unavailable input routes to missing input policy") {
    RuleFixture f;
    f.definition.missing_input_policy = MissingInputPolicy::force_warn;
    f.Load();
    f.Apply(1);
    f.Storage()[0].sample.quality = Quality::unavailable;
    auto result = f.Evaluate();
    CHECK(result.current);
    CHECK(result.quality == Quality::valid);
}
TEST_CASE("2.4 invalid input routes to missing input policy") {
    RuleFixture f;
    f.definition.missing_input_policy = MissingInputPolicy::force_warn;
    f.Load();
    f.Apply(1);
    f.Storage()[0].sample.quality = Quality::invalid;
    auto result = f.Evaluate();
    CHECK(result.current);
    CHECK(result.quality == Quality::valid);
}
TEST_CASE("2.4 age unknown input evaluates and sets age_unknown") {
    RuleFixture f;
    f.Load();
    f.Apply(400);
    f.Storage()[0].sample.age_evidence = AgeEvidence::unknown;
    auto result = f.Evaluate();
    CHECK(result.current);
    CHECK(result.age_unknown);
    CHECK(result.quality == Quality::valid);
}
TEST_CASE("2.4 reference to a missing signal returns the declared missing input result") {
    RuleFixture f;
    f.Load();
    auto result = f.rule.Evaluate({}, 0);
    CHECK(result.quality == Quality::unavailable);
    CHECK(std::isnan(result.value));
}
TEST_CASE("2.4 multiplicative arithmetic composes units") {
    RuleFixture f;
    std::array<ExpressionNode, 7> nodes{{{Operation::equal, 0, Unit::dimensionless, 0, 1, 2},
                                         {Operation::multiply, 0, Unit::dimensionless, 0, 3, 2},
                                         {Operation::multiply, 0, Unit::dimensionless, 0, 5, 2},
                                         {Operation::literal, 2, Unit::pascal},
                                         {Operation::literal, 3, Unit::kelvin},
                                         {Operation::literal, 3, Unit::kelvin},
                                         {Operation::literal, 2, Unit::pascal}}};
    f.definition.nodes = nodes;
    f.Load();
    CHECK(f.Evaluate().current);
    nodes[2].operation = Operation::divide;
    CHECK(f.rule.Load(f.definition, f.signals).code == ErrorCode::incompatible_units);
}
TEST_CASE("2.4 literal without an explicit unit is a load error") {
    RuleFixture f;
    f.nodes[2].literal_unit = kLiteralUnitOmitted;
    CHECK(f.rule.Load(f.definition, f.signals).code == ErrorCode::missing_literal_unit);
}
TEST_CASE("rule operation whitelist evaluates all operations") {
    std::size_t index = 0;
    const double expected[] = {8, 4, 12, 3, 0, 0, 1, 1, 0, 1, 1, 1, 2, 6};
    for (auto op : {Operation::add, Operation::subtract, Operation::multiply, Operation::divide, Operation::less,
                    Operation::less_or_equal, Operation::greater, Operation::greater_or_equal, Operation::equal,
                    Operation::not_equal, Operation::logical_and, Operation::logical_or, Operation::minimum,
                    Operation::maximum}) {
        RuleFixture f;
        f.nodes[0].operation = op;
        f.nodes[1] = {Operation::literal, 6, Unit::dimensionless};
        f.nodes[2] = {Operation::literal, 2, Unit::dimensionless};
        f.Load();
        CHECK(f.Evaluate().quality == Quality::valid);
        CHECK(f.Evaluate().value == expected[index++]);
    }
    for (auto op : {Operation::logical_not, Operation::absolute_value}) {
        RuleFixture f;
        f.nodes[0].operation = op;
        f.nodes[0].child_count = 1;
        f.nodes[1] = {Operation::literal, -2, Unit::dimensionless};
        f.definition.nodes = {f.nodes.data(), 2};
        f.Load();
        CHECK(f.Evaluate().quality == Quality::valid);
    }
    RuleFixture f;
    std::array<ExpressionNode, 4> nodes{{{Operation::clamp, 0, Unit::dimensionless, 0, 1, 3},
                                         {Operation::literal, 8, Unit::dimensionless},
                                         {Operation::literal, 1, Unit::dimensionless},
                                         {Operation::literal, 3, Unit::dimensionless}}};
    f.definition.nodes = nodes;
    f.Load();
    CHECK(f.Evaluate().value == 3);
}
