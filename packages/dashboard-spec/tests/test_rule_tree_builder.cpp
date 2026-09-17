#include "TestSupport.h"
#include "dashboard_spec/RuleTreeBuilder.h"
TEST_CASE("builder preserves node pointers and contiguous immediate children") {
    ds::Document document;
    REQUIRE(ds::BoundedParse(Fixture("valid/hysteresis-debounce.json"), document).Ok());
    ds::RuleTreeBuilder builder;
    const ds::SignalBinding binding{"temperature", 7};
    REQUIRE(builder.Build(document, "r", 42, std::span(&binding, 1)).Ok());
    const auto &rule = builder.Rule();
    REQUIRE(rule.nodes.size() == 3);
    CHECK(rule.rule_id == 42);
    CHECK(rule.nodes[0].operation == signal_core::Operation::greater);
    CHECK(rule.nodes[0].first_child == 1);
    CHECK(rule.nodes[0].child_count == 2);
    CHECK(rule.nodes[1].operation == signal_core::Operation::signal_reference);
    CHECK(rule.nodes[1].signal == 7);
    CHECK(rule.nodes[2].literal_unit == signal_core::Unit::degree_celsius);
    CHECK(std::string(rule.nodes[2].source_pointer) == "/dashboard/rules/r/expression/args/1");
    CHECK(rule.debounce == std::chrono::milliseconds(10));
    CHECK(rule.hysteresis_band == 2);
}
TEST_CASE("all whitelist operations and maximum tree convert") {
    ds::RuleTreeBuilder builder;
    ds::Document document;
    REQUIRE(ds::BoundedParse(Fixture("valid/every-operation.json"), document).Ok());
    const ds::SignalBinding binding{"warning", 1};
    for (const auto &name : ds::Keys(document.Data().json["dashboard"]["rules"]))
        CHECK(builder.Build(document, name, 1, std::span(&binding, 1)).Ok());
    REQUIRE(ds::BoundedParse(Fixture("valid/rule-nodes-512.json"), document).Ok());
    REQUIRE(builder.Build(document, "r", 1, {}).Ok());
    CHECK(builder.Rule().nodes.size() == ds::limits::rule_nodes);
    for (const auto &node : builder.Rule().nodes)
        if (node.child_count)
            CHECK(static_cast<std::size_t>(node.first_child) + node.child_count <= builder.Rule().nodes.size());
}
