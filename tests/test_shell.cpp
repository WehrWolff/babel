#include <boost/process.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <source_location>

#define BUILDING_TESTS
#include "../src/shell.cpp"

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

namespace SharedContext {
    inline std::unique_ptr<Parser> globalParser = nullptr;
    inline std::unique_ptr<std::filesystem::path> g_binPath = nullptr;

    inline void init(const std::filesystem::path& bin) {
        if (!globalParser && !g_binPath) {
            globalParser = std::make_unique<Parser>(loadParserData());
            g_binPath = std::make_unique<std::filesystem::path>(bin);
        }
    }
}

void runBabelWorker(const std::filesystem::path& absolutePath) {
    namespace bp = boost::process::v2;

    // Open a new context and module.
    TheContext = std::make_unique<llvm::LLVMContext>();
    TheModule = std::make_unique<llvm::Module>("Babel Core", *TheContext);

    // Create a new builder for the module.
    Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);

    uintmax_t size = std::filesystem::file_size(absolutePath);
    std::string content(size, '\0');
    std::ifstream in(absolutePath);
    in.read(content.data(), size);

    Lexer lexer = setupModuleAndLexer(absolutePath.string());
    run(lexer, *SharedContext::globalParser, content);

    std::error_code EC;
    std::filesystem::path exePath(PROJECT_ROOT / absolutePath.stem().string().append("_babel"));
    std::filesystem::path outPath(PROJECT_ROOT / absolutePath.stem().string().append(".ll"));
    llvm::raw_fd_ostream outFile(outPath.string(), EC);
    TheModule->print(outFile, nullptr);

    SCOPED_TRACE(absolutePath.string());
    bool isBroken = llvm::verifyModule(*TheModule, &llvm::errs());
    EXPECT_FALSE(isBroken);

    int status = 0;

    std::filesystem::path externPath(PROJECT_ROOT / "externs.o");
    if (!std::filesystem::exists(externPath)) {
        status &= std::system(std::format("clang++ -c {} -o {}", (PROJECT_ROOT / "src" / "externs.cpp").string(), externPath.string()).c_str());
    }

    status &= std::system(std::format("clang {} {} -o {}", outPath.string(), externPath.string(), exePath.string()).c_str());

    // Inject the null stream instead of stdin, so executables awaiting input exit gracefully
    boost::asio::io_context ctx;
    bp::process c(ctx, exePath.string(), {}, bp::process_stdio{.in = nullptr});
    status &= c.wait();

    EXPECT_EQ(status, 0);
    if (status != 0 || isBroken)
        babel_panic("errors occurred while compiling");
}

bool runBabelFileIsolated(const std::filesystem::path& executablePath, const std::filesystem::path& targetFile) {
    namespace bp = boost::process::v2;
    try {
        boost::asio::io_context ctx;
        // Re-invoke this current test binary inside a completely isolated child process
        // Passing the secret '--run-soft-file' flag prevents it from running gtest suites again
        bp::process c(ctx.get_executor(), executablePath.string(), {"--run-soft-file", targetFile.string()});

        // Wait for process resolution and return exit status
        return c.wait() == 0;
    } catch (...) {
        return false;
    }
}

constexpr std::array strictPaths{
    "examples",
    "tests",
};

bool isStrict(const std::filesystem::path& relativePath) {
    if (relativePath.empty())
        return false;

    const auto firstComponent = (*relativePath.begin()).string();
    return std::ranges::find(strictPaths, firstComponent) != strictPaths.end();
}

std::vector<std::filesystem::path> findBabelFiles(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> files;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".babel")
            continue;

        files.push_back(std::filesystem::relative(entry.path(), dir));
    }

    return files;
}

std::set<std::filesystem::path> findStrictDirectories(const std::filesystem::path& root) {
    std::set<std::filesystem::path> dirs;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.path().extension() != ".babel")
            continue;

        auto rel = std::filesystem::relative(entry.path(), root);

        if (!isStrict(rel))
            continue;

        dirs.insert(rel.parent_path());
    }

    return dirs;
}

class BabelDirectoryTest : public testing::Test {
    std::filesystem::path directory;

    public:
        explicit BabelDirectoryTest(std::filesystem::path directory) : directory(std::move(directory)) {}

        void SetUp() override {
            if (std::system(nullptr) == 0 || std::system("clang --version") != 0)
                GTEST_SKIP() << "skipping: clang is not available";
        }

        void TestBody() override {
            for (const auto& file : findBabelFiles(directory)) {
                SCOPED_TRACE(file.generic_string());

                EXPECT_EXIT(
                    {
                        runBabelWorker(std::filesystem::absolute(directory / file));
                        std::exit(0);
                    },
                    ::testing::ExitedWithCode(0),
                    ""
                );
            }
        }
};

void registerStrictDirectory(const std::string& suite, const std::string& name, const std::filesystem::path& directory) {
    ::testing::RegisterTest(
        suite.c_str(),
        name.c_str(),
        nullptr,
        nullptr,
        std::source_location::current().file_name(),
        std::source_location::current().line(),
        [directory]() -> testing::Test* {
            return new BabelDirectoryTest(directory);
        }
    );
}

void registerStrictTests() {
    auto dirs = findStrictDirectories(PROJECT_ROOT);

    for (const auto& relDir : dirs) {
        std::string name = relDir.generic_string();
        std::ranges::replace(name, '/', '.');
        registerStrictDirectory("CompilationTest", name, PROJECT_ROOT / relDir);
    }
}

class CorpusListener : public testing::EmptyTestEventListener {
    std::vector<std::filesystem::path> failures;
    std::size_t totalFiles = 0;

    void runCorpusScan() {
        if (!std::filesystem::exists(PROJECT_ROOT)) return;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(PROJECT_ROOT)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".babel") continue;

            auto rel = std::filesystem::relative(entry.path(), PROJECT_ROOT);
            if (isStrict(rel)) continue;

            totalFiles++;

            // Pass the binary path to Boost.Process so it can execute itself securely
            if (!runBabelFileIsolated(*SharedContext::g_binPath, entry.path())) {
                failures.push_back(rel);
            }
        }
    }

public:
    void OnTestProgramEnd(const testing::UnitTest&) override {
        runCorpusScan();
        
        if (totalFiles == 0 || failures.empty()) {
            std::cout << "\n[CORPUS]\nAll " << totalFiles << " local files compiled successfully.\n";
            return;
        }

        std::ranges::sort(failures);
            
        std::cout
            << '\n'
            << "[CORPUS WARNING]\n"
            << "Encountered compilation errors for "
            << failures.size()
            << '/'
            << totalFiles
            << " local files:\n\n";
        
        for (auto const& file : failures)
            std::cout << "  " << file.generic_string() << '\n';
        
        std::cout << '\n';
    }
};

int main(int argc, char **argv) {
    auto binPath = std::filesystem::absolute(std::filesystem::path(argv[0]));
    SharedContext::init(binPath);
    
    // INTERCEPT: If this is an isolated subprocess invocation for a soft check
    if (argc >= 3 && std::string(argv[1]) == "--run-soft-file") {
        try {
            runBabelWorker(std::filesystem::absolute(argv[2]));
            return 0; // Success code returned to parent process tracker
        } catch (...) {
            return 1; // Explicit abnormal termination failure reporting back
        }
    }
    
    ::testing::InitGoogleTest(&argc, argv);
    // ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    registerStrictTests();

    auto* corpusListener = new CorpusListener();
    testing::UnitTest::GetInstance()->listeners().Append(corpusListener);

    return RUN_ALL_TESTS();
}
