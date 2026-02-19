/* Skeleton and definitions for generating a LALR(1) parser in C++ */
%skeleton "lalr1.cc" 
%defines
%define parse.error verbose
%define api.value.type variant
%define api.token.constructor

/* Required code included before the parser definition begins */
%code requires{
  #include <string>
  #include "Node.h"
  #define USE_LEX_ONLY false //change this macro to true if you want to isolate the lexer from the parser.
}

/* Code included in the parser implementation file */
%code{
  #define YY_DECL yy::parser::symbol_type yylex()
  YY_DECL;
  
  Node* root;
  extern int yylineno;
}

/* Token definitions for the grammar */
/* Tokens represent the smallest units of the language, like operators and parentheses */
%token <std::string> PLUSOP MINUSOP MULTOP INT FLOAT STRING LP RP ID FOR INT_EXPR FLOAT_EXPR VOID BOOL_EXPR
LB RB CLB CRB DOT COMMA COLON
IF ELSE PRINT READ RETURN BREAK CONTINUE 
AND OR LESSOP MOREOP LESSEQOP MOREEQOP COMPOP NOTEQOP DIVOP POWEROP NEGATIONOP ASSIGNOP
TRUE FALSE
NEWLINE 
CLASS MAIN VOLATILE


%token END 0 "end of file"

/* Operator precedence and associativity rules */
/* Used to resolve ambiguities in parsing expressions See https://www.gnu.org/software/bison/manual/bison.html#Precedence-Decl */ 
%left PLUSOP MINUSOP 
%left MULTOP

/* Add these lines to handle the dangling else conflict */
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE


/* Specify types for non-terminals in the grammar */
/* The type specifies the data type of the values associated with these non-terminals */
%type <Node *> root expression factor stmt 
stmts entry stmtBl expressions baseType type var vars
method params class 

/* Grammar rules section */
/* This section defines the production rules for the language being parsed */
%%


/*
program 
class 
entry
method 
var   
type  
baseType 
*/

root:     entry       {root = $1;}
          ;

program

class:          CLASS ID CLB vars method CRB {
                $$ = new Node("Class", "", yylineno);
                $$->children.push_back($4);
                $$->children.push_back($5);
                }


vars:            var {
                    $$ = new Node("VarDecl", "", yylineno);
                    $$->children.push_back($1);
                }

                | vars var {
                    $$ = new Node("VarDecl", "", yylineno);
                    $$->children.push_back($2);
                }

method:         ID LP params RP COLON type stmtBl {
                    $$ = new Node("Method", $1, yylineno);
                    $$->children.push_back($3); /* params */
                    $$->children.push_back($6); /* return type */
                    $$->children.push_back($7); /* body */
                }
                | ID LP RP COLON type stmtBl {
                    /* no parameters */
                    $$ = new Node("Method", $1, yylineno);
                    $$->children.push_back($5); /* return type */
                    $$->children.push_back($6); /* body */
                }
                ;

params:         ID COLON type {
                    /* first/only param - the mandatory one */
                    $$ = new Node("Params", "", yylineno);
                    Node* param = new Node("Param", $1, yylineno);
                    param->children.push_back($3);
                    $$->children.push_back(param);
                }
                | params COMMA ID COLON type {
                    /* additional params - the optional repeating part */
                    Node* param = new Node("Param", $3, yylineno);
                    param->children.push_back($5);
                    $1->children.push_back(param);
                    $$ = $1;
                }
                ;

var:            VOLATILE ID COLON type ASSIGNOP expression {
                /* volatile variable with initialization*/
                $$ = new Node("VarDecl", $2, yylineno);
                $$->value = "volatile";
                $$->children.push_back($4); /* child 0: type */
                $$->children.push_back($6); /* child 1: initialization expression */
                }
                | VOLATILE ID COLON type {
                    /* volatile variable without initialization */
                    $$ = new Node("VarDecl", $2, yylineno);
                    $$->value = "volatile";
                    $$->children.push_back($4); /* child 0: type */
                }
                | ID COLON type ASSIGNOP expression {
                    /* normal variable with initialization */
                    $$ = new Node("VarDecl", $1, yylineno);
                    $$->children.push_back($3); /* child 0: type */
                    $$->children.push_back($5); /* child 1: initialization expression */
                }
                | ID COLON type {
                    /* normal variable without initialization */
                    $$ = new Node("VarDecl", $1, yylineno);
                    $$->children.push_back($3); /* child 0: type */
                }
                ;
                
type:            baseType LB RB {
                    $$ = new Node("ArrayType", "", yylineno);
                    $$->children.push_back($1);
                }
                |  baseType {
                    $$ = $1;
                }
                |  ID {
                    $$ = new Node("UserType", $1, yylineno);
                }
                |  VOID {
                    $$ = new Node("VoidType", "", yylineno);
                }
                ;

baseType:         INT_EXPR {
                     $$ = new Node("IntType", "int", yylineno); 
                }
                | FLOAT_EXPR {
                    $$ = new Node("FloatType", "float", yylineno);
                }

                | BOOL_EXPR {
                    $$ = new Node("BoolType", "boolean", yylineno); 
                }
                ;


expressions:      expression{ 
                $$ = new Node("expression", "", yylineno);
                $$->children.push_back($1);
            }
            | expressions expression NEWLINE{ 
                $1->children.push_back($2);
                $$ = $1;
            }
;

         

expression: expression PLUSOP expression {      /*
                                                  Create a subtree that corresponds to the AddExpression
                                                  The root of the subtree is AddExpression
                                                  The childdren o
                                                  f the AddExpression subtree are the left hand side (expression accessed through $1) and right hand side of the expression (expression accessed through $3)
                                                */
                            $$ = new Node("AddExpression", "", yylineno);
                            $$->children.push_back($1);
                            $$->children.push_back($3);
                            /* printf("r1 "); */
                          }
            | expression MINUSOP expression {
                            $$ = new Node("SubExpression", "", yylineno);
                            $$->children.push_back($1);
                            $$->children.push_back($3);
                            /* printf("r2 "); */
                          }
            | expression MULTOP expression {
                            $$ = new Node("MultExpression", "", yylineno);
                            $$->children.push_back($1);
                            $$->children.push_back($3);
                            /* printf("r3 "); */
                          }


            | factor      {$$ = $1;  /*printf("r4 ");*/}
            ;

factor: 
             INT                {  $$ = new Node("Int", $1, yylineno); /* printf("r5 ");  Here we create a leaf node Int. The value of the leaf node is $1 */}
            | FLOAT             {  $$ = new Node("Float", $1, yylineno); /* printf("r5.1 "); */}
            | STRING            {  
                                  std::string s = $1;
                                  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
                                      s = s.substr(1, s.size() - 2);
                                  }
                                  $$ = new Node("String", s, yylineno); 
                                }

            | LP expression RP  {  $$ = $2; /* printf("r6 ");  simply return the expression */}
            | ID                {  $$ = new Node("ID", $1, yylineno); /* printf("r7 ");*/}
            | TRUE              {  $$ = new Node("Bool", $1, yylineno); }
            | FALSE             {  $$ = new Node("Bool", $1, yylineno); }
            
            /*boools kommer in här med, typ 2<5 är en bool expr*/
            ;


stmt:       PRINT LP expression RP {
                          $$ = new Node("PrintStatement", "", yylineno);
                          $$->children.push_back($3);
                          }

            | RETURN expression {
                          $$ = new Node("ReturnStatement", "", yylineno);
                          $$->children.push_back($2);
                          }

            | BREAK {
                  $$ = new Node("BreakStatement", "", yylineno);
                  }

            | CONTINUE {
                      $$ = new Node("ContinueStatement", "", yylineno);
                      }

            | IF LP expression RP stmt %prec LOWER_THAN_ELSE {
                              $$ = new Node("IfStatement", "", yylineno);
                              $$->children.push_back($3);
                              $$->children.push_back($5);
                              }

            | IF LP expression RP stmt ELSE stmt {
                              $$ = new Node("IfElseStatement", "", yylineno);
                              $$->children.push_back($3);
                              $$->children.push_back($5);
                              $$->children.push_back($7);
                            }


            | expressions {$$ = $1;}
            
            | var {$$ = $1;}

            ;

stmts:      stmt NEWLINE{ 
                $$ = new Node("Statements", "", yylineno);
                $$->children.push_back($1);
            }
            | stmts stmt NEWLINE{ 
                $1->children.push_back($2);
                $$ = $1;
            }
;

stmtBl:     CLB NEWLINE stmts CRB {  /*statments Boys Love*/
                 $$ = $3;
            }
            | CLB  stmts CRB {
              $$ = $2;
            }


entry:      MAIN LP RP COLON INT_EXPR stmtBl {
                        $$ = new Node("MainStatement", "", yylineno);
                        $$->children.push_back($6);
                        }
            ;



/*                    
          | READ LP expression RP NEWLINE {
                          $$ = new Node("ReadStatement", "", yylineno);
                          $$->children.push_back($3);
                          }


*/

%%