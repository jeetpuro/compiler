%top{
    #include "parser.tab.hh"
    #define YY_DECL yy::parser::symbol_type yylex()
    #include "Node.h"
    int lexical_errors = 0;
}
%option yylineno noyywrap nounput batch noinput stack 
%%

"+"                     {if(USE_LEX_ONLY) {printf("PLUSOP ");} else {return yy::parser::make_PLUSOP(yytext);}}
"-"                     {if(USE_LEX_ONLY) {printf("SUBOP ");} else {return yy::parser::make_MINUSOP(yytext);}}
"*"                     {if(USE_LEX_ONLY) {printf("MULTOP ");} else {return yy::parser::make_MULTOP(yytext);}}
"("                     {if(USE_LEX_ONLY) {printf("LP ");} else {return yy::parser::make_LP(yytext);}}
")"                     {if(USE_LEX_ONLY) {printf("RP ");} else {return yy::parser::make_RP(yytext);}}
"["                     {if(USE_LEX_ONLY) {printf("LB ");} else {return yy::parser::make_LB(yytext);}}
"]"                     {if(USE_LEX_ONLY) {printf("RB ");} else {return yy::parser::make_RB(yytext);}}
"{"                     {if(USE_LEX_ONLY) {printf("CLB ");} else {return yy::parser::make_CLB(yytext);}}
"}"                     {if(USE_LEX_ONLY) {printf("CRB ");} else {return yy::parser::make_CRB(yytext);}}
":"                     {if(USE_LEX_ONLY) {printf("COLON ");} else {return yy::parser::make_COLON(yytext);}}
"."                     {if(USE_LEX_ONLY) {printf("DOT ");} else {return yy::parser::make_DOT(yytext);}}
","                     {if(USE_LEX_ONLY) {printf("COMMA ");} else {return yy::parser::make_COMMA(yytext);}}


"&"                     {if(USE_LEX_ONLY) {printf("AND ");} else {return yy::parser::make_AND(yytext);}}
"|"                     {if(USE_LEX_ONLY) {printf("OR ");} else {return yy::parser::make_OR(yytext);}}
"<"                     {if(USE_LEX_ONLY) {printf("LESSOP ");} else {return yy::parser::make_LESSOP(yytext);}}
">"                     {if(USE_LEX_ONLY) {printf("MOREOP ");} else {return yy::parser::make_MOREOP(yytext);}}
"<="                    {if(USE_LEX_ONLY) {printf("LESSEQOP ");} else {return yy::parser::make_LESSEQOP(yytext);}}
">="                    {if(USE_LEX_ONLY) {printf("MOREEQOP ");} else {return yy::parser::make_MOREEQOP(yytext);}}
"="                     {if(USE_LEX_ONLY) {printf("COMPOP ");} else {return yy::parser::make_COMPOP(yytext);}}
"!="                    {if(USE_LEX_ONLY) {printf("NOTEQOP ");} else {return yy::parser::make_NOTEQOP(yytext);}}
"/"                     {if(USE_LEX_ONLY) {printf("DIVOP ");} else {return yy::parser::make_DIVOP(yytext);}}
"^"                     {if(USE_LEX_ONLY) {printf("POWEROP ");} else {return yy::parser::make_POWEROP(yytext);}}
":="                    {if(USE_LEX_ONLY) {printf("ASSIGNOP ");} else {return yy::parser::make_ASSIGNOP(yytext);}}
"!"                     {if(USE_LEX_ONLY) {printf("NOT ");} else {return yy::parser::make_NOT(yytext);}}


0|[1-9][0-9]*           {if(USE_LEX_ONLY) {printf("INT ");} else {return yy::parser::make_INT(yytext);}}
[0-9]+\.[0-9]+          {if(USE_LEX_ONLY) {printf("FLOAT ");} else {return yy::parser::make_FLOAT(yytext);}}
"for"                   {if(USE_LEX_ONLY) {printf("FOR ");} else {return yy::parser::make_FOR(yytext);}}
"int"                   {if(USE_LEX_ONLY) {printf("INT_EXPR ");} else {return yy::parser::make_INT_EXPR(yytext);}}
"float"                 {if(USE_LEX_ONLY) {printf("FLOAT_EXPR ");} else {return yy::parser::make_FLOAT_EXPR(yytext);}}
"void"                  {if(USE_LEX_ONLY) {printf("VOID ");} else {return yy::parser::make_VOID(yytext);}}
\"[^\"]*\"              {if(USE_LEX_ONLY) {printf("STRING ");} else {return yy::parser::make_STRING(yytext);}}
"if"                    {if(USE_LEX_ONLY) {printf("IF ");} else {return yy::parser::make_IF(yytext);}}
"else"                  {if(USE_LEX_ONLY) {printf("ELSE ");} else {return yy::parser::make_ELSE(yytext);}}
"print"                 {if(USE_LEX_ONLY) {printf("PRINT ");} else {return yy::parser::make_PRINT(yytext);}}
"read"                  {if(USE_LEX_ONLY) {printf("READ ");} else {return yy::parser::make_READ(yytext);}}
"return"                {if(USE_LEX_ONLY) {printf("RETURN ");} else {return yy::parser::make_RETURN(yytext);}}
"break"                 {if(USE_LEX_ONLY) {printf("BREAK ");} else {return yy::parser::make_BREAK(yytext);}}
"continue"              {if(USE_LEX_ONLY) {printf("CONTINUE ");} else {return yy::parser::make_CONTINUE(yytext);}}
"boolean"               {if(USE_LEX_ONLY) {printf("BOOL_EXPR ");} else {return yy::parser::make_BOOL_EXPR(yytext);}}
"true"                  {if(USE_LEX_ONLY) {printf("TRUE ");} else {return yy::parser::make_TRUE(yytext);}}
"false"                 {if(USE_LEX_ONLY) {printf("FALSE ");} else {return yy::parser::make_FALSE(yytext);}}
"main"                  {if(USE_LEX_ONLY) {printf("MAIN ");} else {return yy::parser::make_MAIN(yytext);}}
"class"                 {if(USE_LEX_ONLY) {printf("CLASS ");} else {return yy::parser::make_CLASS(yytext);}}
"volatile"              {if(USE_LEX_ONLY) {printf("VOLATILE ");} else {return yy::parser::make_VOLATILE(yytext);}}
"length"                {if(USE_LEX_ONLY) {printf("LENGTH ");} else {return yy::parser::make_LENGTH(yytext);}}
\n+                     {if(USE_LEX_ONLY) {printf("NEWLINE ");} else {return yy::parser::make_NEWLINE(yytext);}}



[a-zA-Z"%0-9$@_]*        {if(USE_LEX_ONLY) {printf("ID ");} else {return yy::parser::make_ID(yytext);}}


[ \t\r]+                {}
"//"[^\n]*              {}
.                       { if(!lexical_errors) fprintf(stderr, "Lexical errors found! See the logs below: \n"); fprintf(stderr,"\t@error at line %d. Character %s is not recognized\n", yylineno, yytext); lexical_errors = 1;}
<<EOF>>                 {return yy::parser::make_END();}
%%