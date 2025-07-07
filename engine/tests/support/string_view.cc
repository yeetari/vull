#include <vull/support/string_view.hh>

#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

#include <stdint.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(StringView, ToIntegral_EmptyString) {
    EXPECT_THAT(StringView("").to_integral<int32_t>(), is(null()));
    EXPECT_THAT(StringView("").to_integral<uint32_t>(), is(null()));
    EXPECT_THAT(StringView(" ").to_integral<uint32_t>(), is(null()));
}

TEST_CASE(StringView, ToIntegral_Malformed) {
    EXPECT_THAT(StringView("+").to_integral<int32_t>(), is(null()));
    EXPECT_THAT(StringView("-").to_integral<int32_t>(), is(null()));
    EXPECT_THAT(StringView("++").to_integral<int32_t>(), is(null()));
    EXPECT_THAT(StringView("--").to_integral<int32_t>(), is(null()));
    EXPECT_THAT(StringView("+").to_integral<uint32_t>(), is(null()));
    EXPECT_THAT(StringView("-").to_integral<uint32_t>(), is(null()));
    EXPECT_THAT(StringView("++").to_integral<uint32_t>(), is(null()));
    EXPECT_THAT(StringView("--").to_integral<uint32_t>(), is(null()));

    EXPECT_THAT(StringView("a").to_integral<uint32_t>(), is(null()));
    EXPECT_THAT(StringView("-a").to_integral<int32_t>(), is(null()));
}

TEST_CASE(StringView, ToIntegral_Unsigned) {
    // Random cases.
    EXPECT_THAT(StringView("0").to_integral<uint32_t>(), is(equal_to(0u)));
    EXPECT_THAT(StringView("10").to_integral<uint32_t>(), is(equal_to(10u)));
    EXPECT_THAT(StringView("500").to_integral<uint32_t>(), is(equal_to(500u)));
    EXPECT_THAT(StringView("67564").to_integral<uint32_t>(), is(equal_to(67564u)));
    EXPECT_THAT(StringView("010").to_integral<uint32_t>(), is(equal_to(10u)));

    // Limit cases.
    EXPECT_THAT(StringView("255").to_integral<uint8_t>(), is(equal_to(255)));
    EXPECT_THAT(StringView("65535").to_integral<uint16_t>(), is(equal_to(65535)));
    EXPECT_THAT(StringView("4294967295").to_integral<uint32_t>(), is(equal_to(4294967295u)));
    EXPECT_THAT(StringView("18446744073709551615").to_integral<uint64_t>(), is(equal_to(18446744073709551615uz)));
}

TEST_CASE(StringView, ToIntegral_Signed) {
    // Random cases.
    EXPECT_THAT(StringView("0").to_integral<int32_t>(), is(equal_to(0)));
    EXPECT_THAT(StringView("-5").to_integral<int32_t>(), is(equal_to(-5)));
    EXPECT_THAT(StringView("-50").to_integral<int32_t>(), is(equal_to(-50)));
    EXPECT_THAT(StringView("-67564").to_integral<int32_t>(), is(equal_to(-67564)));
    EXPECT_THAT(StringView("-020").to_integral<int32_t>(), is(equal_to(-20)));

    // Limits cases in both directions.
    EXPECT_THAT(StringView("127").to_integral<int8_t>(), is(equal_to(127)));
    EXPECT_THAT(StringView("32767").to_integral<int16_t>(), is(equal_to(32767)));
    EXPECT_THAT(StringView("2147483647").to_integral<int32_t>(), is(equal_to(2147483647)));
    EXPECT_THAT(StringView("9223372036854775807").to_integral<int64_t>(), is(equal_to(9223372036854775807z)));
    EXPECT_THAT(StringView("-128").to_integral<int8_t>(), is(equal_to(-128)));
    EXPECT_THAT(StringView("-32768").to_integral<int16_t>(), is(equal_to(-32768)));
    EXPECT_THAT(StringView("-2147483648").to_integral<int32_t>(), is(equal_to(-2147483648)));
    EXPECT_THAT(StringView("-9223372036854775808").to_integral<int64_t>(), is(equal_to(INT64_MIN)));
}

TEST_CASE(StringView, ToIntegral_Overflow) {
    // Random cases.
    EXPECT_THAT(StringView("50000").to_integral<uint8_t>(), is(null()));
    EXPECT_THAT(StringView("70000").to_integral<uint16_t>(), is(null()));
    EXPECT_THAT(StringView("-500").to_integral<int8_t>(), is(null()));

    // Unsigned limit cases.
    EXPECT_THAT(StringView("256").to_integral<uint8_t>(), is(null()));
    EXPECT_THAT(StringView("65536").to_integral<uint16_t>(), is(null()));
    EXPECT_THAT(StringView("4294967296").to_integral<uint32_t>(), is(null()));
    EXPECT_THAT(StringView("18446744073709551616").to_integral<uint64_t>(), is(null()));

    // Signed limit cases in both directions.
    EXPECT_THAT(StringView("128").to_integral<int8_t>(), is(null()));
    EXPECT_THAT(StringView("32768").to_integral<int16_t>(), is(null()));
    EXPECT_THAT(StringView("2147483648").to_integral<int32_t>(), is(null()));
    EXPECT_THAT(StringView("9223372036854775808").to_integral<int64_t>(), is(null()));
    EXPECT_THAT(StringView("-129").to_integral<int8_t>(), is(null()));
    EXPECT_THAT(StringView("-32769").to_integral<int16_t>(), is(null()));
    EXPECT_THAT(StringView("-2147483649").to_integral<int32_t>(), is(null()));
    EXPECT_THAT(StringView("-9223372036854775809").to_integral<int64_t>(), is(null()));
}
