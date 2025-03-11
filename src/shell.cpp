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

void run(const Lexer& lexer, const Parser& parser, const std::string& text) {
    std::list<Token> tokens = lexer.tokenize(text);
    std::cout << parser.parse(tokens) << std::endl;
}

void saveParserData(const Parser& parser, const std::filesystem::path& filename) {
    std::ofstream ofs(filename, std::ios::binary);
    boost::archive::binary_oarchive ar(ofs);
    ar << parser;
}

Parser loadParserData(const std::filesystem::path& project_root) {
    Parser parser;
    std::filesystem::path dataPath = project_root / "assets" / "parser.dat";

    if (auto ifs = std::ifstream(dataPath, std::ios::binary)/*; false*/) {
        boost::archive::binary_iarchive ar(ifs);
        ar >> parser;
    } else {
        std::filesystem::path grammarPath = project_root / "build" / "grammar.txt";
        std::ifstream t(grammarPath);
        if (!t.is_open()) { std::cout << "Error opening file" << std::endl; }
        std::stringstream buffer;
        buffer << t.rdbuf();

        Grammar grammar(transform_string(buffer.str()));
        std::cout << grammar.alphabet << std::endl;
        LRClosureTable closureTable(grammar);
        LRTable lrTable(closureTable);
        parser = Parser(lrTable);
        saveParserData(parser, dataPath);
    }
    return parser;
}

Lexer setupModuleAndLexer(const std::string& file_name) {
    // Open a new context and module.
    // TheContext = std::make_unique<LLVMContext>();
    // TheModule = std::make_unique<Module>("Babel Core", *TheContext);

    // Create a new builder for the module.
    // Builder = std::make_unique<IRBuilder<>>(*TheContext);

    auto lexer = Lexer(file_name, {
        {"TYPE", "\\b(?:int|float|bool|string|char|list|tuple|map|dict|any|void)\\b"},
        {"CLASS", "\\bclass\\b"},
        {"TASK", "\\btask\\b"},
        {"STRUCT", "\\bstruct\\b"},
        {"STORAGE_MODIFIER", "\\b(?:static|const|final)\\b"},
        {"STRING", R"("[^"]*")"},
        {"CHAR", "'[^']{1}'"},
        {"FLOATING_POINT", "\\d*\\.\\d+"},
        {"BOOL", "(TRUE|FALSE)"},
        {"LPAREN", "\\("},
        {"LSQUARE", "\\["},
        {"RSQUARE", "\\]"},
        {"LBRACE", "\\{"},
        {"RBRACE", "\\}"},
        {"RPAREN", "\\)"},
        {"IF", "if"},
        {"ELSE", "else"},
        {"ELIF", "elif"},
        {"THEN", "then"},
        {"MATCH", "macth"},
        {"CASE", "case"},
        {"OTHERWISE", "otherwise"},
        {"END", "end"},
        {"DO", "do"},
        {"WHILE", "while"},
        {"FOR", "for"},
        {"TO", "to"},
        {"STEP", "step"},
        {"TRY", "try"},
        {"CATCH", "catch"},
        {"FINALLY", "finally"},
        {"PASS", "pass"},
        {"CONTINUE", "continue"},
        {"BREAK", "break"},
        {"RETURN", "return"},
        {"RAISE", "raise"},
        {"IMPORT", "imp"},    
        {"EQEQ", "=="},
        {"PLUS_EQUALS", "\\+="},
        {"MINUS_EQUALS", "-="},
        {"MULTIPLY_EQUALS", "\\*="},
        {"DIVIDE_EQUALS", "/="},
        {"POWER_EQUALS", "\\^="},
        {"MODULO_EQUALS", "%="},
        {"INTEGER_DIVIDE_EQUALS", "//="},
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
        {"POWER", "\\^"},
        {"MODULO", "%"},
        {"EQUALS", "="},
        {"OR", "\\|"},
        {"AND", "&"},
        {"NOT", "!"},
        {"LT", "<"},
        {"GT", ">"},
        {"DOT", "\\."},
        {"COMMA", ","},
        {"COLON", ":"},
        {"SEMICOLON", ";"},
        {"NEWLINE", "\n"},
        {"NULL", "null"},
        {"NEW", "new"},
        {"VAR", "[a-zA-Z_][a-zA-Z0-9_]*"},
        {"INTEGER", "\\d*"} //leave INTEGER here, it matches all expressions
    });

    return lexer;
}

int main(int argc, char* argv[]) {
    Lexer lexer = setupModuleAndLexer("repl");
    const std::filesystem::path ROOT_DIR = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();
    Parser parser = loadParserData(ROOT_DIR);

    std::cout << R"( _____       _          _   |  Documentation: https://github.com/WehrWolff/babel/wiki)" << "\n";
    std::cout << R"(| ___ \     | |        | |  |                                                        )" << "\n";
    std::cout << R"(| |_/ / __ _| |__   ___| |  |  Use bemo for managing packages                        )" << "\n";
    std::cout << R"(| ___ \/ _` | '_ \ / _ \ |  |                                                        )" << "\n";
    std::cout << R"(| |_/ / (_| | |_) |  __/ |  |  Version UNRELEASED (Mar 28, 2024)                     )" << "\n";
    std::cout << R"(\____/ \__,_|_.__/ \___|_|  |  https://github.com/WehrWolff/babel                    )" << "\n\n";

    while (true) {
        std::string text;
        std::cout << color::rize("babel> ", color::BOLD, color::MAGENTA);
        getline(std::cin, text);
        
        if (text == "exit()") break;
        run(lexer, parser, text);
    }

    return 0;
}
