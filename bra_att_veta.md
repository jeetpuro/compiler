för att söka efter problem almänt dude 
bison -Wcounterexamples parser.yy

* vi behöver göra test cases för varje non terminal. dvs om class{} och det fattas ett "}" så ska det stå: "syntax error missing a CRB"

* bison shiftar alltid som default, så kanske inte behöver ändra shift reduce som handlar om function delen

* så vad som var ett fel var, expressions non terminalen (kolla rad 238). här och vi hade det i statements som vi har "| factor {$$ = $1;  /*printf("r4 ");*/} " i expressions. i grammar rad 19 säger vi att vi bara tillåter expression sen slut. inga mer expression på samma rad.

----------------------------------------------------------------------------

var:            VOLATILE ID COLON type ASSIGNOP expression {
                /* volatile variable with initialization*/
                    $$ = new Node("VarDecl", $2, yylineno);
                    /*$$->value = "volatile";*/


* hittade ett fel där Vardecl NODE i trädet hette "Vardecl:volatile". men här ska namnet av variablen vara. det som händer är att /*$$->value = "volatile";*/ uppdaterar namnet result till volatile.

har lagt extra child

KOM IHÅG TILL SEMANTIC ANALYS nedan är copy från claude

Note: For volatile vars, children shift by one (child 0 = Volatile, child 1 = type, child 2 = init). Keep this in mind when accessing children later in semantic analysis.


------------------------------



* rad 461, det stog stmt istället för stmtBl detta gjorde att parsern inte förväntade sig en {} efter for statementen


-------------------------


* vi behöver lösa när det ser ut såhär, titta test 6:

                class{}

                main {}

-------------------------------------------



        if (j < (i + 1)) {
          cont := false
        }
        this or
                if (j < (i + 1)) cont := false


-----------------------------------------------
[a-zA-Z"%$@_]*        {if(USE_LEX_ONLY) {printf("ID "

är det såhär det ska se ut?

--------------LAB 2 --------------------------------------------------


verify fals positives
for f in valid/*.cpm; do echo "$f:"; ./compiler "$f" 2>&1 | grep "error\|passed"; done

not expression liggeer  i faktor delen i parsen, ska den verkligen vara  där`?

NotExpression delen i sematic använder en funtion som jämnför 2 nodes, me nfrån negationexpression får vi bara 1 node. detta kan bli problem i framtiden


 
---------------LAB 3 -------------------------------------------------