//#include "lexer.h"
#include "lrparser.h"
#include "colormod.h"
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <fstream>
#include <string>
#include <sstream>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <filesystem>

#include "tools.h"

void run(const Lexer& lexer, const Parser& parser, const std::string& text) {
    std::vector<Token> tokens = lexer.tokenize(text);
    Lexer::handleComments(tokens);
    Lexer::insertSemicolons(tokens);
    std::cout << tokens << std::endl;
    // std::visit([](const auto& value) { /* std::cout << value << std::endl; */ }, parser.parse(tokens));
    std::variant<TreeNode, std::string> out = parser.parse(tokens);
    
    if (std::holds_alternative<std::string>(out))
        std::cout << std::get<std::string>(out) << '\n';
}

Parser loadParserData(const std::filesystem::path& project_root) {
    std::filesystem::path grammarPath = project_root / "build" / "grammar.txt";
    std::ifstream t(grammarPath);
    if (!t.is_open()) { std::cout << "Error opening file" << std::endl; }
    std::stringstream buffer;
    buffer << t.rdbuf();

    Grammar grammar(transform_string(buffer.str()));
    LRClosureTable closureTable(grammar);
    LRTable lrTable(closureTable);
    Parser parser(lrTable);
    
    return parser;
}

Lexer setupModuleAndLexer(const std::string& file_name) {
    auto lexer = Lexer(file_name, {
        {"TYPE", "\\b(?:int|float|bool|string|cstr|char|list|tuple|map|dict|any|void)\\b"},
        {"CLASS", "\\bclass\\b"},
        {"EXTERN", "\\bextern\\b"},
        {"TASK", "\\btask\\b"},
        {"STRUCT", "\\bstruct\\b"},
        {"COMMENT", R"(\\\\.*)"},
        {"LET", "\\blet\\b"},
        {"CONST", "\\bconst\\b"},
        {"CSTRING", R"~(c"(\\.|[^"\\])*")~"},
        {"STRING", R"~("(\\.|[^"\\])*")~"},
        {"CHAR", "'[^']{1}'"},
        {"BOOL", "(TRUE|FALSE)"},
        {"LPAREN", "\\("},
        {"LSQUARE", "\\["},
        {"RSQUARE", "\\]"},
        {"LBRACE", "\\{"},
        {"RBRACE", "\\}"},
        {"RPAREN", "\\)"},
        {"IF", "\\bif\\b"},
        {"ELSE", "\\belse\\b"},
        {"ELIF", "\\belif\\b"},
        {"THEN", "\\bthen\\b"},
        {"MATCH", "\\bmatch\\b"},
        {"CASE", "\\bcase\\b"},
        {"OTHERWISE", "\\botherwise\\b"},
        {"END", "\\bend\\b"},
        {"DO", "\\bdo\\b"},
        {"WHILE", "\\bwhile\\b"},
        {"FOR", "\\bfor\\b"},
        {"TO", "\\bto\\b"},
        {"STEP", "\\bstep\\b"},
        {"TRY", "\\btry\\b"},
        {"CATCH", "\\bcatch\\b"},
        {"FINALLY", "\\bfinally\\b"},
        {"NOOP", "\\bnoop\\b"},
        {"CONTINUE", "\\bcontinue\\b"},
        {"BREAK", "\\bbreak\\b"},
        {"GOTO", "\\bgoto\\b"},
        {"LABEL_START", "\\$"},
        {"RETURN", "\\breturn\\b"},
        {"RAISE", "\\braise\\b"},
        {"IMPORT", "\\bimp\\b"},    
        {"EQEQ", "=="},
        {"PLUS_EQUALS", "\\+="},
        {"MINUS_EQUALS", "-="},
        {"MULTIPLY_EQUALS", "\\*="},
        {"DIVIDE_EQUALS", "/="},
        {"POWER_EQUALS", "\\*\\*="},
        {"MODULO_EQUALS", "%="},
        {"INTEGER_DIVIDE_EQUALS", "//="},
        {"LSHIFT_EQUALS", "<<="},
        {"RSHIFT_EQUALS", ">>="},
        {"BIT_OR_EQUALS", "\\|="},
        {"BIT_AND_EQUALS", "&="},
        {"BIT_XOR_EQUALS", "\\^="},
        {"NEGLIGIBLY_LOW", "<<<"},
        {"LSHIFT", "<<"},
        {"RSHIFT", ">>"},
        {"LTEQ", "<="},
        {"GTEQ", ">="},
        {"NOTEQ", "!="},
        {"RARR","=>"},
        {"INTEGER_DIVIDE", "//"},
        {"INCREMENT", "\\+\\+"},
        {"DECREMENT", "--"},
        {"PLUS", "\\+"},
        {"MINUS", "-"},
        {"MULTIPLY", "\\*"},
        {"DIVIDE", "/"},
        {"POWER", "\\*\\*"},
        {"MODULO", "%"},
        {"EQUALS", "="},
        {"OR", "\\|\\|"},
        {"XOR", "\\^\\^"},
        {"AND", "&&"},
        {"BIT_OR", "\\|"},
        {"BIT_XOR", "\\^"},
        {"BIT_AND", "&"},
        {"NOT", "!"},
        {"LT", "<"},
        {"GT", ">"},
        {"COMMA", ","},
        {"COLON", ":"},
        {"SEMICOLON", ";"},
        {"NEWLINE", "\n"},
        {"NULL", "null"},
        {"NEW", "new"},
        {"FLOATING_POINT", "\\b(?:NaN|Inf)(?:_[HhFfDdQq])?\\b"},
        {"VAR", "[a-zA-Z_][a-zA-Z0-9_]*"},
        {"FLOATING_POINT", "\\b[0-9](?:[0-9']*[0-9])?[eE][+-]?[0-9](?:[0-9']*[0-9])?(?:_?[HhFfDdQq])?\\b"}, // leave these before integer, so the first part is not matched as one
        {"FLOATING_POINT", "\\b[0-9](?:[0-9']*[0-9])?\\.[0-9](?:[0-9']*[0-9])?(?:[eE][+-]?[0-9](?:[0-9']*[0-9])?)?(?:_?[HhFfDdQq])?\\b"},
        {"FLOATING_POINT", "\\b0x[0-9A-Fa-f](?:[0-9A-Fa-f']*[0-9A-Fa-f])?[pP][+-]?[0-9](?:[0-9']*[0-9])?(?:_[HhFfDdQq])?\\b"}, // hex versions
        {"FLOATING_POINT", "\\b0x[0-9A-Fa-f](?:[0-9A-Fa-f']*[0-9A-Fa-f])?\\.[0-9A-Fa-f](?:[0-9A-Fa-f']*[0-9A-Fa-f])?(?:[pP][+-]?[0-9](?:[0-9']*[0-9])?)?(?:_[HhFfDdQq])?\\b"},
        {"FLOATING_POINT", "\\b(?:0[ob])?[0-9](?:[0-9']*[0-9])?_?[HhFfDdQq]\\b"}, // before int, so for example 10f is not matched as one
        {"INTEGER", "\\b(?:0[xob])?[0-9A-Fa-f](?:[0-9A-Fa-f']*[0-9A-Fa-f])?(?:_?[BbSsIiLlCc])?\\b"}, // leave this here so something like abc1 is matched as a variable
        {"FLOATING_POINT", "\\b0x[0-9A-Fa-f](?:[0-9A-Fa-f']*[0-9A-Fa-f])?_[HhFfDdQq]\\b"}, // same as above, also int needs to be matched first (0xff)
        {"FLOATING_POINT", "\\.[0-9](?:[0-9']*[0-9])?(?:[eE][+-]?[0-9](?:[0-9']*[0-9])?)?(?:_?[HhFfDdQq])?"}, // these may be after integer, since the dot distinguishes them immediately
        {"FLOATING_POINT", "0x\\.[0-9A-Fa-f](?:[0-9A-Fa-f']*[0-9A-Fa-f])?(?:[pP][+-]?[0-9](?:[0-9']*[0-9])?)?(?:_[HhFfDdQq])?"},
        {"DOT", "\\."} // needs to be matched after fp (.5)
    });

    return lexer;
}

int main(int argc, char* argv[]) {
    // Open a new context and module.
    TheContext = std::make_unique<llvm::LLVMContext>();
    TheModule = std::make_unique<llvm::Module>("Babel Core", *TheContext);

    // Create a new builder for the module.
    Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);

    if (argc == 1) {
        Lexer lexer = setupModuleAndLexer("repl");
        const std::filesystem::path ROOT_DIR = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();
        Parser parser = loadParserData(ROOT_DIR);

        std::cout << R"( _____       _          _   |  Documentation: https://github.com/WehrWolff/babel/wiki)" << "\n";
        std::cout << R"(| ___ \     | |        | |  |                                                        )" << "\n";
        std::cout << R"(| |_/ / __ _| |__   ___| |  |  Use beemo for managing packages                       )" << "\n";
        std::cout << R"(| ___ \/ _` | '_ \ / _ \ |  |                                                        )" << "\n";
        std::cout << R"(| |_/ / (_| | |_) |  __/ |  |  Version UNRELEASED (Mar 28, 2024)                     )" << "\n";
        std::cout << R"(\____/ \__,_|_.__/ \___|_|  |  https://github.com/WehrWolff/babel                    )" << "\n\n";

        while (true) {
            std::string text;
            std::cout << color::rize("babel> ", color::FORMAT_CODE::BOLD, color::FORMAT_CODE::MAGENTA);
            getline(std::cin, text);
            
            if (text == "exit()") break;
            run(lexer, parser, text);
        }
    } else if (argc == 2)
    {
        std::filesystem::path source = std::filesystem::absolute(argv[1]);

        unsigned long size = std::filesystem::file_size(source);
        std::string content(size, '\0');
        std::ifstream in(source);
        in.read(&content[0], size);

        Lexer lexer = setupModuleAndLexer(argv[1]);
        const std::filesystem::path ROOT_DIR = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();
        Parser parser = loadParserData(ROOT_DIR);

        run(lexer, parser, content);

        std::error_code EC;
        llvm::raw_fd_ostream outFile(source.stem().string() + ".ll", EC);

        if (EC) {
            llvm::errs() << "Error opening file: " << EC.message() << "\n";
        } else {
            TheModule->print(outFile, nullptr);
        }
    }

    llvm::outs() << "=== LLVM IR Dump ===\n";
    TheModule->print(llvm::outs(), nullptr);
    llvm::verifyModule(*TheModule, &llvm::errs());
    
    return 0;
}
