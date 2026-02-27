#include <iostream>
#include "parser.tab.hh"
#include <cstring>

extern Node *root;
extern FILE *yyin;
extern int yylineno;
extern int lexical_errors;
extern int syntax_errors;
extern yy::parser::symbol_type yylex();

bool debug_mode = true;

enum errCodes
{
	SUCCESS = 0,
	LEXICAL_ERROR = 1,
	SYNTAX_ERROR = 2,
	AST_ERROR = 3,
	SEMANTIC_ERROR = 4,
	SEGMENTATION_FAULT = 139
};

int errCode = errCodes::SUCCESS;

// Handling Syntax Errors
void yy::parser::error(std::string const &err)
{
	if (!lexical_errors)
	{
        // std::cerr << "DEBUG: " << err << std::endl;
        std::cerr << "Syntax errors found! See the logs below:" << std::endl;

        if (debug_mode)
            std::cerr << "\tDEBUG raw bison error: \"" << err << "\"" << std::endl;


        // Map of {pattern to find, message to print}
        static const std::vector<std::pair<std::vector<std::string>, std::string>> errorPatterns = {
            {{"unexpected INT_EXPR", "expecting ID"}, "Illegal class name: identifier expected"},
            {{"unexpected RP", "expecting LB"},          "Invalid print statement: type used as expression"},
            {{"unexpected INT_TYPE", "expecting INT_EXPR"}, "Invalid print statement: type used as expression"},
            {{"unexpected INT_EXPR", "expecting LB"}, "Invalid type declaration: identifier expected"},
            {{"unexpected INT_EXPR"},                 "Invalid type declaration: identifier expected"},
            {{"unexpected $undefined", "expecting CLB"}, "Invalid class signature"},
            {{"unexpected CRB", "expecting"},          "Missing statement after if"},
            {{"expecting CRB"},                        "Missing closing bracket '}'"},
            {{"expecting RP"},                         "Missing closing parenthesis ')'"},
            {{"unexpected RP"},                        "Trailing comma in argument/parameter list"},
            {{"expecting RB"},                         "Missing closing bracket ']'"},
            {{"expecting LP"},                         "Missing opening parenthesis '('"},
            {{"expecting CLB"},                        "Missing opening bracket '{'"},
            {{"expecting COLON"},                      "Missing colon ':' in declaration"},
            {{"expecting ID"},                         "Expected an identifier"},
            {{"expecting CLASS or MAIN"},              "Missing 'main' method"},
            {{"unexpected ASSIGNOP"},                  "Stray assignment operator ':='"},
            {{"unexpected INT_TYPE"},                  "Invalid type declaration: identifier expected"},
            {{"unexpected COMMA"},                     "Invalid method call - unexpected comma"},
            {{"unexpected NEWLINE"},                   "Missing statement or expression"},
            {{"unexpected IF"},                        "Illegal use of 'if' inside an if-condition"},
            {{"unexpected CLASS", "CRB"},              "Missing closing bracket '}'"},
            {{"unexpected CLASS"},                     "Missing closing bracket '}'"},
            {{"unexpected $undefined"},                "Invalid syntax - unexpected token"},
            {{"unexpected VOLATILE"},                  "Illegal access modifier in method declaration"},
        };

        bool matched = false;
        for (const auto& [patterns, message] : errorPatterns)
        {
            // Check if ALL patterns in the vector match
            bool allMatch = true;
            for (const auto& pattern : patterns)
            {
                if (err.find(pattern) == std::string::npos)
                {
                    allMatch = false;
                    break;
                }
            }

            if (allMatch)
            {
                std::cerr << "\t@error at line " << yylineno << ". " << message << std::endl;
                if (debug_mode)
                    std::cerr << "\tDEBUG matched pattern: {\"" << patterns[0] << "\"" 
                              << (patterns.size() > 1 ? ", \"" + patterns[1] + "\"" : "") 
                              << "}" << std::endl;
                matched = true;
                break;
            }
        }

        // Catch-all
        if (!matched)
        {
            std::cerr << "\t@error at line " << yylineno << ". " << err << std::endl;
            if (debug_mode)
                std::cerr << "\tDEBUG no pattern matched - raw error printed" << std::endl;
        }

        errCode = errCodes::SYNTAX_ERROR;
    }
}

int main(int argc, char **argv)
{
    // Reads from file if a file name is passed as an argument. Otherwise, reads from stdin.
    if (argc > 1)
    {
        // Check for -d flag
        for (int i = 1; i < argc; i++)
        {
            if (strcmp(argv[i], "-d") == 0)
                debug_mode = true;
        }

        // Find the first non-flag argument as the input file
        const char *filename = nullptr;
        for (int i = 1; i < argc; i++)
        {
            if (strcmp(argv[i], "-d") != 0)
            {
                filename = argv[i];
                break;
            }
        }

        if (filename && !(yyin = fopen(filename, "r")))
        {
            perror(filename);
            return 1;
        }
    }
    //
    if (USE_LEX_ONLY)
        yylex();
    else
    {
        yy::parser parser;

        bool parseSuccess = !parser.parse();

        if (lexical_errors)
            errCode = errCodes::LEXICAL_ERROR;

        if (parseSuccess && !lexical_errors && syntax_errors == 0)
        {
            printf("\nThe compiler successfuly generated a syntax tree for the given input! \n");

            printf("\nPrint Tree:  \n");
            try
            {
                root->print_tree();
                root->generate_tree();
            }
            catch (...)
            {
                errCode = errCodes::AST_ERROR;
            }
        }
    }

    return errCode;
}