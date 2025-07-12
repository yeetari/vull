#pragma once

#include <vull/support/source_location.hh>
#include <vull/test/matchers.hh>
#include <vull/test/message.hh>
#include <vull/test/test.hh>

namespace vull::test {

bool try_match(const auto &actual, const auto &matcher, StringView expression,
               SourceLocation location = SourceLocation::current()) {
    if (matcher.matches(actual)) {
        return false;
    }

    Message message;
    message.append_text(vull::format("       Actual: '{}'\n", expression));
    message.append_text("     Expected: ");
    matcher.describe(message);
    message.append_text("\n          but: ");
    matcher.describe_mismatch(message, actual);
    message.append_text(vull::format("\n     at {}:{}\n", location.file_name(), location.line()));
    g_current_test->append_message(message.build());
    return true;
}

// clang-format off
#define ASSERT_THAT(actual, matcher) if (vull::test::try_match(actual, matcher, #actual)) return
#define EXPECT_THAT(actual, matcher) vull::test::try_match(actual, matcher, #actual)
// clang-format on

#define ASSERT_FALSE(actual) ASSERT_THAT(actual, is(equal_to(false)))
#define EXPECT_FALSE(actual) EXPECT_THAT(actual, is(equal_to(false)))
#define ASSERT_TRUE(actual) ASSERT_THAT(actual, is(equal_to(true)))
#define EXPECT_TRUE(actual) EXPECT_THAT(actual, is(equal_to(true)))

} // namespace vull::test
