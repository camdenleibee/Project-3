// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// ------------------------------------------------------------- your tests
TEST(reg_empty_table) {
    ToolRegistry r;

    // Check the starting state before adding anything.
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1);
    CHECK(r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0);
    CHECK(r.max_probe_length() == 0);
    CHECK(r.find("missing") == nullptr);
}

TEST(reg_null_rejected) {
    ToolRegistry r;

    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);

    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.size() == 1);

    // Rejecting a null tool should not change the size.
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 1);

    CHECK(r.find("missing") == nullptr);
    CHECK(r.size() == 1);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;

    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    REQUIRE(r.add(std::make_unique<NamedTool>("beta")));
    REQUIRE(r.add(std::make_unique<NamedTool>("gamma")));

    int total = 0;
    int alpha = 0;
    int beta = 0;
    int gamma = 0;

    // Count how many times each tool is visited.
    r.for_each([&](const Tool& tool) {
        ++total;

        if (tool.name() == "alpha") {
            ++alpha;
        } else if (tool.name() == "beta") {
            ++beta;
        } else if (tool.name() == "gamma") {
            ++gamma;
        }
    });

    CHECK(total == 3);
    CHECK(alpha == 1);
    CHECK(beta == 1);
    CHECK(gamma == 1);
}

TEST(reg_10000_tools_and_alternative_hash) {
    ToolRegistry r;
    Tool* first = nullptr;

    for (int i = 0; i < 10000; ++i) {
        std::string name = "tool_" + std::to_string(i);
        REQUIRE(r.add(std::make_unique<NamedTool>(name)));

        // Save this address before any later resizes happen.
        if (i == 0) {
            first = r.find(name);
        }

        CHECK(r.load_factor() <= 1.0);
    }

    CHECK(r.size() == 10000);

    // Resizing should not change the first tool's address.
    REQUIRE(first != nullptr);
    CHECK(r.find("tool_0") == first);

    // Make sure no entries were lost during resizing.
    for (int i = 0; i < 10000; ++i) {
        std::string name = "tool_" + std::to_string(i);
        Tool* tool = r.find(name);

        REQUIRE(tool != nullptr);
        CHECK(tool->name() == name);
    }

    CHECK(r.max_probe_length() <= 64);

    // Count collisions for the weak hash from the spec.
    std::vector<int> counts(r.bucket_count(), 0);

    for (int i = 0; i < 10000; ++i) {
        std::string name = "tool_" + std::to_string(i);

        std::size_t weak_hash =
            static_cast<std::size_t>(name[0]) + name.size();

        std::size_t index = weak_hash % r.bucket_count();
        ++counts[index];
    }

    int weak_longest = 0;

    for (int count : counts) {
        if (count > weak_longest) {
            weak_longest = count;
        }
    }

    // Save these numbers for the design log.
    std::printf(
        "buckets=%zu load=%.6f longest_chain=%zu weak_longest=%d\n",
        r.bucket_count(), r.load_factor(),
        r.max_probe_length(), weak_longest);
}

TEST(ws_accepts_required_paths) {
    TempDir d;
    Workspace ws(d.path());

    struct PathCase {
        const char* input;
        const char* expected;
    };

    // Accepted examples from the spec.
    const PathCase cases[] = {
        {"notes/todo.txt", "notes/todo.txt"},
        {"./notes//todo.txt", "notes/todo.txt"},
        {".", "."},
        {"a/../b", "b"},
        {"a/..", "."},
        {"new/dir/file.txt", "new/dir/file.txt"},
        {"my..notes.txt", "my..notes.txt"}
    };

    for (const PathCase& c : cases) {
        fs::path out;
        std::string reason;

        REQUIRE(ws.resolve(c.input, &out, &reason));

        if (std::string(c.expected) == ".") {
            CHECK(out == ws.root());
        } else {
            CHECK(out == ws.root() / c.expected);
        }
    }

    // resolve should check the path without creating anything.
    CHECK(!fs::exists(ws.root() / "new"));
}

TEST(ws_rejects_required_paths) {
    TempDir d;
    Workspace ws(d.path());

    const char* paths[] = {
        "",
        "/etc/passwd",
        "..",
        "../etc/passwd",
        "a/../../b",
        "notes/../../ws/notes/todo.txt"
    };

    for (const char* path : paths) {
        fs::path out;
        std::string reason;

        CHECK(!ws.resolve(path, &out, &reason));
        CHECK(!reason.empty());
    }

    // An absolute path is still rejected if it points inside root.
    fs::path out;
    std::string reason;

    CHECK(!ws.resolve(ws.root().string(), &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_nul) {
    TempDir d;
    Workspace ws(d.path());

    // Build this explicitly so the NUL stays inside the string.
    std::string path = std::string("notes") + '\0' + "/x";

    fs::path out;
    std::string reason;

    CHECK(!ws.resolve(path, &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_left_to_right) {
    double value = 0;
    std::string error;

    // Equal-precedence operators should run left to right.
    REQUIRE(eval("10 - 4 - 3", &value, &error));
    CHECK(value == 3);

    REQUIRE(eval("100 / 10 / 5", &value, &error));
    CHECK(value == 2);
}

TEST(calc_parentheses) {
    double value = 0;
    std::string error;

    REQUIRE(eval("(2 + 3) * 4", &value, &error));
    CHECK(value == 20);

    REQUIRE(eval("2 * (3 + 4) - 5", &value, &error));
    CHECK(value == 9);
}

TEST(calc_negative_numbers) {
    double value = 0;
    std::string error;

    REQUIRE(eval("-3 * -3", &value, &error));
    CHECK(value == 9);

    REQUIRE(eval("2 * -3", &value, &error));
    CHECK(value == -6);
}

TEST(calc_decimals) {
    double value = 0;
    std::string error;

    // Division should keep the fractional part.
    REQUIRE(eval("7 / 2", &value, &error));
    CHECK(value == 3.5);
}

TEST(calc_division_by_zero_in_parentheses) {
    double value = 0;
    std::string error;

    // The denominator becomes zero after evaluating the parentheses.
    CHECK(!eval("1 / (2 - 2)", &value, &error));
    CHECK(error == "division by zero");
}

TEST(calc_extra_expressions) {
    double value = 0;
    std::string error;

    REQUIRE(eval("(8 - 2) / (1 + 2)", &value, &error));
    CHECK(value == 2);

    REQUIRE(eval("1.5 * 2 + 0.25", &value, &error));
    CHECK(value == 3.25);
}

TEST(calc_long_expression) {
    std::string expression = "1";

    // Build an expression with 20,000 terms.
    for (int i = 1; i < 20000; ++i) {
        expression += "+1";
    }

    double value = 0;
    std::string error;

    REQUIRE(eval(expression, &value, &error));
    CHECK(value == 20000);
}

TEST(calc_deep_parentheses) {
    // Check that deeply nested parentheses work without recursion.
    std::string expression(5000, '(');
    expression += "7";
    expression += std::string(5000, ')');

    double value = 0;
    std::string error;

    REQUIRE(eval(expression, &value, &error));
    CHECK(value == 7);
}

TEST(calc_empty_tokens_rejected) {
    std::vector<CalcToken> tokens;
    double value = 0;
    std::string error;

    // Empty input should return an error instead of reading an empty stack.
    CHECK(!calc_evaluate(tokens, &value, &error));
    CHECK(!error.empty());
}
int main(int argc, char** argv) { return mt::run_all(argc, argv); }
