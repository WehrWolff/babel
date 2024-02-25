//#include "lexer.h"
#include "lrparser.h"
#include <fstream>
#include <string>
#include <sstream>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <filesystem>


/*template<typename T>
ostream& operator<<(ostream& os, const list<T>& myList) {
    os << "{";
    if (!myList.empty()) {
        auto it = myList.begin();
        os << *it;
        ++it;
        for (; it != myList.end(); ++it) {
            os << ", " << *it;
        }
    }
    os << "}";
    return os;
} */

void run(Lexer lexer, Parser parser, std::string text) {
    std::list<Token> tokens = lexer.tokenize(text);
    parser.parse(tokens);
}

void saveParserData(const Parser& parser, const std::filesystem::path& filename) {
    std::ofstream ofs(filename);
    boost::archive::text_oarchive ar(ofs);
    ar << parser;
}

Parser loadParserData(const std::filesystem::path& executablePath) {
    Parser parser;
    std::filesystem::path dataPath = executablePath.parent_path().parent_path() / "assets" / "parser.dat";
    std::cout << dataPath << std::endl;
    std::ifstream ifs(dataPath);

    if (ifs.is_open()) {
        boost::archive::text_iarchive ar(ifs);
        ar >> parser;
    } else {
        std::filesystem::path grammarPath = executablePath.parent_path() / "grammar.txt";
        std::cout << grammarPath << std::endl;
        std::ifstream t(grammarPath);
        if (!t.is_open()) { std::cout << "Error opening file" << std::endl; }
        std::stringstream buffer;
        buffer << t.rdbuf();

        Grammar grammar(transform_string(buffer.str()));
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

    Lexer lexer = Lexer(file_name, {
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
    std::filesystem::path executablePath = std::filesystem::absolute(std::filesystem::path(argv[0]));
    Parser parser = loadParserData(executablePath);    

    while (true) {
        std::string text;
        std::cout << "babel> ";
        getline(std::cin, text);
        
        if (text == "exit()") break;
        run(lexer, parser, text);
    }

    return 0;
}
