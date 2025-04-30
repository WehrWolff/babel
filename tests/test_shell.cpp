#include <gtest/gtest.h>
#include "lrparser.h"

TEST(GrammarTest, AxiomAndRules) {
    Grammar grammar("A' -> A\nA -> a A\nA -> a");
    ASSERT_EQ("A'", grammar.axiom);
    ASSERT_EQ(3, grammar.rules.size());
    const std::list<std::string> a = {"a"};
    ASSERT_EQ(a, grammar.firsts.at("A"));
    ASSERT_EQ(UnifiedItem(Rule(&grammar, "A -> a A"), 1), UnifiedItem(Rule(&grammar, "A -> a A"), 1));
    ASSERT_EQ(0, indexOf(UnifiedItem(Rule(&grammar, "A -> a A"), 1), std::vector{UnifiedItem(Rule(&grammar, "A -> a A"), 1)}));
}

TEST(LRClosureTableTest, ClosureAndKernels) {
    Grammar grammar("A' -> A\nA -> a A\nA -> a");
    LRClosureTable lrClosureTable(grammar);
    ASSERT_EQ(3, lrClosureTable.kernels.front().closure.size());
    ASSERT_EQ(4, lrClosureTable.kernels.size());
}

TEST(LRTableTest, States) {
    Grammar grammar("A' -> A\nA -> a A\nA -> a");
    LRClosureTable lrClosureTable(grammar);
    LRTable lrTable(lrClosureTable);
    ASSERT_EQ(4, lrTable.states.size());
}

TEST(ParserTest, Parse) {
    Grammar grammar("A' -> A\nA -> a A\nA -> a");
    LRClosureTable lrClosureTable(grammar);
    LRTable lrTable(lrClosureTable);
    Parser parser(lrTable);

    std::vector<Token> tokens1 = {Token("a", "a")};
    std::vector<Token> tokens2 = {Token("a", "a"), Token("a", "a")};
    std::vector<Token> tokens3 = {Token("a", "a"), Token("b", "b")};

    auto result1 = parser.parse(tokens1);
    auto result2 = parser.parse(tokens2);
    auto result3 = parser.parse(tokens3);

    ASSERT_TRUE(std::holds_alternative<TreeNode>(result1));
    ASSERT_TRUE(std::holds_alternative<TreeNode>(result2));
    ASSERT_TRUE(std::holds_alternative<std::string>(result3));
    ASSERT_EQ("SyntaxError: Expected 'a' or EOF but found 'b'", std::get<std::string>(result3));
}

TEST(GrammarTest, AnotherGrammar) {
    Grammar grammar1("A' -> A\nA -> B\nA -> ''\nB -> ( A )");
    ASSERT_EQ("A'", grammar1.axiom);
    ASSERT_EQ(4, grammar1.rules.size());
    const std::list<std::string> a1 = {"''", "("};
    ASSERT_EQ(a1, grammar1.firsts.at("A"));
}

TEST(LRClosureTableTest, AnotherClosureTable) {
    Grammar grammar1("A' -> A\nA -> B\nA -> ''\nB -> ( A )");
    LRClosureTable lrClosureTable1(grammar1);
    ASSERT_EQ(4, lrClosureTable1.kernels.front().closure.size());
    ASSERT_EQ(10, lrClosureTable1.kernels.size());
}

TEST(LRTableTest, AnotherLRTable) {
    Grammar grammar1("A' -> A\nA -> B\nA -> ''\nB -> ( A )");
    LRClosureTable lrClosureTable1(grammar1);
    LRTable lrTable1(lrClosureTable1);
    ASSERT_EQ(10, lrTable1.states.size());
    ASSERT_EQ("s3", lrTable1.states[0].mapping.at("(").toString());
    ASSERT_EQ("r2", lrTable1.states[0].mapping.at("$").toString());
    ASSERT_EQ("r0", lrTable1.states[1].mapping.at("$").toString());
    ASSERT_EQ("4", lrTable1.states[3].mapping.at("A").toString());
    ASSERT_EQ("r3", lrTable1.states[9].mapping.at(")").toString());
}

TEST(ParserTest, AnotherParse) {
    Grammar grammar1("A' -> A\nA -> B\nA -> ''\nB -> ( A )");
    LRClosureTable lrClosureTable1(grammar1);
    LRTable lrTable1(lrClosureTable1);
    Parser parser1(lrTable1);

    std::vector<Token> tokens1 = {Token("(", "("), Token(")", ")")};
    std::vector<Token> tokens2 = {Token("(", "("), Token("(", "("), Token(")", ")"), Token(")", ")")};
    std::vector<Token> tokens3 = {Token("(", "("), Token(")", ")"), Token("(", "("), Token(")", ")")};

    auto result1 = parser1.parse(tokens1);
    auto result2 = parser1.parse(tokens2);
    auto result3 = parser1.parse(tokens3);

    ASSERT_TRUE(std::holds_alternative<TreeNode>(result1));
    ASSERT_TRUE(std::holds_alternative<TreeNode>(result2));
    ASSERT_TRUE(std::holds_alternative<std::string>(result3));
    ASSERT_EQ("SyntaxError: Expected EOF but found '('", std::get<std::string>(result3));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
