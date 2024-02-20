//#include "lexer.h"
#include "lrparser.h"

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

void run (std::string file_name, std::string text) {
    // Open a new context and module.
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("Babel Core", *TheContext);

    // Create a new builder for the module.
    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    Lexer lexer = Lexer(file_name, text); //construct with file name, text

    std::list<std::pair<std::string, std::string>> token_specs = {
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
        {"INTEGER_DIVIDE", "//"},
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
    };
    
    lexer.setSpecs(token_specs);
    
    std::list<Token> tokens = lexer.tokenize();

    std::ifstream t("grammar.txt");
    std::stringstream buffer;
    buffer << t.rdbuf();

    Grammar grammar(transform_string(buffer.str()));
    LRClosureTable closureTable(grammar);
    LRTable lrTable(closureTable);

    Parser parser(lrTable, tokens);
    parser.parse();    
}

int main() {
    while(true){
        std::string text;
        std::cout << "babel> ";
        getline(std::cin, text);
        
        if (text == "exit()") break;
        run("repl", text);
    }
    return 0;
}
