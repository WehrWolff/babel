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
    Lexer lexer = Lexer(file_name, text); //construct with file name, text

    std::list<std::pair<std::string, std::string>> token_specs = {
        {"TYPE", "\\b(?:int|double|bool|string|char|list|tuple|map|dict|any|void|null|class|task)\\b"},
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
        {"THEN", "then"},
        {"END", "end"},
        {"DO", "do"},
        {"WHILE", "while"},
        {"FOR", "for"},
        {"TO", "to"},
        {"CONTINUE", "continue"},
        {"BREAK", "break"},
        {"RETURN", "return"},
        {"IMPORT", "imp"},    
        {"EQEQ", "=="},
        {"PLUS_EQUALS", "\\+="},
        {"MINUS_EQUALS", "-="},
        {"MULTIPLY_EQUALS", "\\*="},
        {"DIVIDE_EQUALS", "/="},
        {"POWER_EQUALS", "\\^="},
        {"MODULO_EQUALS", "%="},
        {"INTEGER_DIVIDE_EQUALS", "//="},
        {"NEGLIGIBLY_LOW", "<<"},
        {"LTEQ", "<="},
        {"GTEQ", ">="},
        {"NOTEQ", "!="},
        {"INTEGER_DIVIDE", "//"},
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
        {"VAR", "[a-zA-Z_][a-zA-Z0-9_-]*"},
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

int main(){
    while(true){
        std::string text;
        std::cout << "babel> ";
        getline(std::cin, text);
        
        if (text == "exit()") break;
        run("repl", text);
    }
    return 0;
}
