#include <iostream>
#include "parser.tab.hh"

extern Node *root;
extern FILE *yyin;
extern int yylineno;
extern int lexical_errors;
extern int syntax_errors;
extern yy::parser::symbol_type yylex();

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

        // Map of {pattern to find, message to print}
        // ORDER MATTERS - first match wins
        static const std::vector<std::pair<std::vector<std::string>, std::string>> errorPatterns = {
            // {patterns that must ALL be present},  message
            {{"unexpected RP", "expecting LB"},       "Invalid print statement: type used as expression"},
            {{"unexpected INT_EXPR", "expecting LB"}, "Invalid type declaration: identifier expected"},
            {{"unexpected INT_EXPR"},                 "Invalid type declaration: identifier expected"},
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
            {{"unexpected INT_TYPE"},                  "Invalid print statement: type used as expression"},
            {{"unexpected COMMA"},                     "Invalid method call - unexpected comma"},
            {{"unexpected NEWLINE"},                   "Missing statement or expression"},
            {{"unexpected IF"},                        "Illegal use of 'if' inside an if-condition"},
            {{"unexpected CLASS"},                     "Invalid class signature"},
            {{"unexpected $undefined"},                "Invalid syntax - unexpected token"},
            {{"unexpected VOLATILE"},                  "Illegal access modifier in method declaration"},        
            {{"unexpected CRB", "expecting"},          "Missing statement after if"},    
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
                matched = true;
                break;
            }
        }

        // Catch-all
        if (!matched)
        {
            std::cerr << "\t@error at line " << yylineno << ". " << err << std::endl;
        }

        errCode = errCodes::SYNTAX_ERROR;
    }
}

int main(int argc, char **argv)
{
	// Reads from file if a file name is passed as an argument. Otherwise, reads from stdin.
	if (argc > 1)
	{
		if (!(yyin = fopen(argv[1], "r")))
		{
			perror(argv[1]);
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