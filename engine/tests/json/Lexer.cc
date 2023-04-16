#include <vull/json/Lexer.hh>

#include <vull/json/Token.hh>
#include <vull/maths/Epsilon.hh>
#include <vull/support/Assert.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Test.hh>

using namespace vull;

TEST_SUITE(JsonLexer, {
    ;
    TEST_CASE(Empty) {
        json::Lexer lexer("");
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(Whitespace) {
        json::Lexer lexer("        ");
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(Null) {
        json::Lexer lexer("null");
        EXPECT(lexer.next().kind() == json::TokenKind::Null);
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(True) {
        json::Lexer lexer("true");
        EXPECT(lexer.next().kind() == json::TokenKind::True);
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(False) {
        json::Lexer lexer("false");
        EXPECT(lexer.next().kind() == json::TokenKind::False);
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(Punctuation) {
        json::Lexer lexer("{}[]:,");
        EXPECT(lexer.next().kind() == json::TokenKind::ObjectBegin);
        EXPECT(lexer.next().kind() == json::TokenKind::ObjectEnd);
        EXPECT(lexer.next().kind() == json::TokenKind::ArrayBegin);
        EXPECT(lexer.next().kind() == json::TokenKind::ArrayEnd);
        EXPECT(lexer.next().kind() == json::TokenKind::Colon);
        EXPECT(lexer.next().kind() == json::TokenKind::Comma);
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(Integer) {
        json::Lexer lexer("1234");
        auto token = lexer.next();
        EXPECT(token.kind() == json::TokenKind::Number);
        EXPECT(vull::fuzzy_equal(token.number(), 1234.0));
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(NegativeInteger) {
        json::Lexer lexer("-1234");
        auto token = lexer.next();
        EXPECT(token.kind() == json::TokenKind::Number);
        EXPECT(vull::fuzzy_equal(token.number(), -1234.0));
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(Decimal) {
        json::Lexer lexer("1234.56");
        auto token = lexer.next();
        EXPECT(token.kind() == json::TokenKind::Number);
        EXPECT(vull::fuzzy_equal(token.number(), 1234.56));
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(NegativeDecimal) {
        json::Lexer lexer("-1234.56");
        auto token = lexer.next();
        EXPECT(token.kind() == json::TokenKind::Number);
        EXPECT(vull::fuzzy_equal(token.number(), -1234.56));
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(Exponent) {
        json::Lexer lexer("1234e5 -1234.56E5");
        auto first = lexer.next();
        EXPECT(first.kind() == json::TokenKind::Number);
        EXPECT(vull::fuzzy_equal(first.number(), 1234e5));
        auto second = lexer.next();
        EXPECT(second.kind() == json::TokenKind::Number);
        EXPECT(vull::fuzzy_equal(second.number(), -1234.56e5));
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(NegativeExponent) {
        json::Lexer lexer("1234e-5 -1234.56E-5");
        auto first = lexer.next();
        EXPECT(first.kind() == json::TokenKind::Number);
        EXPECT(vull::fuzzy_equal(first.number(), 1234e-5));
        auto second = lexer.next();
        EXPECT(second.kind() == json::TokenKind::Number);
        EXPECT(vull::fuzzy_equal(second.number(), -1234.56e-5));
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(EmptyString) {
        json::Lexer lexer("\"\"");
        auto token = lexer.next();
        EXPECT(token.kind() == json::TokenKind::String);
        EXPECT(token.string().empty());
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(String) {
        json::Lexer lexer("\"foo\"");
        auto token = lexer.next();
        EXPECT(token.kind() == json::TokenKind::String);
        EXPECT(token.string() == "foo");
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }

    TEST_CASE(MalformedString) {
        json::Lexer lexer("\"foo");
        auto token = lexer.next();
        EXPECT(token.kind() == json::TokenKind::Invalid);
        EXPECT(lexer.next().kind() == json::TokenKind::Eof);
    }
})
