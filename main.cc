#include <iostream>
#include "parser.tab.hh"

extern Node *root;
extern FILE *yyin;
extern int yylineno;
extern int lexical_errors;
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
        std::cerr << "Syntax errors found! See the logs below:" << std::endl;

        // ClassMissingClosingBracket.cpm / MethodMissingClosingBracket.cpm
        if (err.find("expecting CRB") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Missing closing bracket '}'" << std::endl;
        }
        // UnmatchedParentheses.cpm / MissingMethodBraces.cpm
        else if (err.find("expecting RP") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Missing closing parenthesis ')'" << std::endl;
        }
        // Missing closing square bracket
        else if (err.find("expecting RB") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Missing closing bracket ']'" << std::endl;
        }
        // MissingMethodBraces.cpm - missing opening parenthesis
        else if (err.find("expecting LP") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Missing opening parenthesis '('" << std::endl;
        }
        // Missing opening bracket
        else if (err.find("expecting CLB") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Missing opening bracket '{'" << std::endl;
        }
        // InvalidMethodDeclaration.cpm / InvalidTypeDeclaration.cpm - expecting colon
        else if (err.find("expecting COLON") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Missing colon ':' in declaration" << std::endl;
        }
        // IllegalClassName.cpm / InvalidClassSignature.cpm - expecting ID after class
        else if (err.find("expecting ID") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Expected an identifier (name)" << std::endl;
        }
        // NoMainMethod.cpm - expecting MAIN
        else if (err.find("expecting MAIN") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Missing 'main' method" << std::endl;
        }
        // ExtraToken.cpm - unexpected ASSIGNOP
        else if (err.find("unexpected ASSIGNOP") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Stray assignment operator ':='" << std::endl;
        }
        // InvalidPrintStatement.cpm - unexpected token in print
        else if (err.find("unexpected PRINT") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Invalid print statement" << std::endl;
        }
        // InvalidMethodCall.cpm / InvalidMethodCall1.cpm / InvalidMethodCall2.cpm
        else if (err.find("unexpected COMMA") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Invalid method call - unexpected comma" << std::endl;
        }
        // MissingStatementAfterIf.cpm
        else if (err.find("unexpected NEWLINE") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Missing statement or expression" << std::endl;
        }
        // IllegalIfInIfCondition.cpm
        else if (err.find("unexpected IF") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Illegal use of 'if' inside an if-condition" << std::endl;
        }
        // InvalidClassSignature.cpm - unexpected CLASS
        else if (err.find("unexpected CLASS") != std::string::npos)
        {
            std::cerr << "\t@error at line " << yylineno << ". Invalid class signature" << std::endl;
        }
        // Catch-all for any other syntax error
        else
        {
            std::cerr << "\t@error at line " << yylineno << ". " << err << std::endl;
        }
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

		if (parseSuccess && !lexical_errors)
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