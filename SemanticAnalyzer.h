#ifndef SEMANTICANALYZER_H
#define SEMANTICANALYZER_H

#include "Node.h"
#include "SymbolTable.h"
#include <iostream>

using namespace std;

// ─────────────────────────────────────────────
// SemanticAnalyzer: walks the AST with the symbol table
// and performs all semantic checks.
//
// Node.h stays a pure data structure.
// All semantic rules live HERE.
// ─────────────────────────────────────────────
class SemanticAnalyzer {
public:
    SymbolTable st;
    int errors = 0;

private:
    string currentMethodRetType = "";  // declared return type of the method being analysed
    string currentClassName = "";       // class currently being analysed
    // className → (methodName → returnType): populated by preScanClasses() before
    // the main analysis pass so dot-calls can resolve forward-referenced classes.
    map<string, map<string, string>> classMethods;
    // className → (methodName → [paramTypes]): populated by preScanClasses()
    map<string, map<string, vector<string>>> classMethodParams;

public:

    // ── Entry point ──────────────────────────────────────────────

    void analyze(Node* root) {
        if (!root) return;
        preScanClasses(root);   // must run first so all class method types are known
        buildSymbolTable(root);
        st.generate_dot();
    }

    // ── Symbol Table Construction + Semantic Checks ──────────────

    string buildSymbolTable(Node* node) {
        if (!node) return "";

        node->visited = true;  // mark as checked for the semantic debugger

        const string& type  = node->type;
        const string& value = node->value; // kan döpas om till name

        if (type == "Program") {

            // Pre-register every class name into global scope so that forward
            // references (e.g. `volatile d: classdata` before classdata is declared)
            // are visible when VarDecl checks the type.
            for (auto* child : node->children) {
                if (!child) continue;
                // The Classes node holds the list of Class nodes
                if (child->type == "Classes") {
                    for (auto* cls : child->children) {
                        if (cls && cls->type == "Class")
                            st.put(cls->value, new Record(cls->value, "class", "class"));
                    }
                } else if (child->type == "Class") {
                    st.put(child->value, new Record(child->value, "class", "class"));
                }
            }

            for (auto* child : node->children)
                if (child) buildSymbolTable(child);

        } else if (type == "Class") {
            // Class name is already pre-registered above; only report duplicate if
            // st.put fails AND the existing record was NOT put by the pre-scan
            // (i.e. there is a genuine re-declaration by the user).
            // We skip re-inserting here — just open the scope.
            string savedClassName = currentClassName;
            currentClassName = value;
            st.enterScope("class:" + value);

            // Pre-register all method signatures so that forward calls within
            // the class (e.g. a1 calling a2 before a2 is declared) resolve correctly.
            for (auto* child : node->children) {
                if (!child || child->type != "Methods") continue;
                for (auto* method : child->children) {
                    if (!method || method->type != "Method") continue;
                    string retType = "unknown";
                    for (auto* mc : method->children) {
                        if (mc && (mc->type == "Type" || mc->type == "ArrayType")) {
                            retType = getTypeStr(mc);
                            break;
                        }
                    }
                    if (!st.put(method->value, new Record(method->value, "method", retType))) {
                        reportError(method, "Already Declared Function: '" + method->value + "'");
                    }
                }
            }

            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
            st.exitScope();
            currentClassName = savedClassName;

        } else if (type == "Method") {
            // Signature already pre-registered in the Class handler above.
            // Resolve this method's declared return type so ReturnStatement can check it.
            string retType = "unknown";
            for (auto* child : node->children) {
                if (child && (child->type == "Type" || child->type == "ArrayType")) {
                    retType = getTypeStr(child);
                    break;
                }
            }
            string savedRetType = currentMethodRetType;
            currentMethodRetType = retType;
            st.enterScope("method:" + value);
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
            st.exitScope();
            currentMethodRetType = savedRetType;

            // Check for missing return in non-void method
            if (retType != "void" && retType != "unknown") {
                Node* stmtsNode = nullptr;
                for (auto* child : node->children)
                    if (child && child->type == "Statements") { stmtsNode = child; break; }
                if (stmtsNode && !hasReturn(stmtsNode)) {
                    reportError(node, "Missing return statement in non-void method '" + value + "'");
                }
            }

        } else if (type == "Param") {
            // children[0] is the Type node
            string typeStr = "unknown";
            if (!node->children.empty() && node->children.front())
                typeStr = getTypeStr(node->children.front());
            if (!st.put(value, new Record(value, "parameter", typeStr))) {
                reportError(node, "Already Declared parameter: '" + value + "'");
            }
            // No need to recurse: Param's only child is a Type node

        } else if (type == "VarDecl" && !value.empty()) {
            // Leaf VarDecl (value = identifier name). Find type node.
            string typeStr = "unknown";
            for (auto* child : node->children) {
                if (child && (child->type == "Type" || child->type == "ArrayType")) {
                    typeStr = getTypeStr(child);
                    break;
                }
            }
            if (!st.put(value, new Record(value, "variable", typeStr))) {
                reportError(node, "Already Declared variable: '" + value + "'");
            }
            // If the declared type looks like a class name (not a primitive built
            // into the grammar), verify that it is actually declared as a class.
            // We detect primitives by what the grammar produces for baseType nodes:
            // "int", "float", "boolean", "void", "unknown" — everything else must
            // be a user-declared class.
            {
                string baseType = typeStr;
                if (baseType.size() > 2 && baseType.substr(baseType.size()-2) == "[]")
                    baseType = baseType.substr(0, baseType.size()-2);
                // Grammar primitives — no lookup needed
                bool isPrimitive = (baseType == "int" || baseType == "float" ||
                                    baseType == "boolean" || baseType == "void" ||
                                    baseType == "unknown");
                if (!isPrimitive) {
                    Record* cls = st.lookup(baseType);
                    if (!cls || cls->kind != "class")
                        reportError(node, "Undeclared type: '" + baseType + "'");
                }
            }

        } else if (type == "MainStatement") {
            st.enterScope("main");
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
            st.exitScope();
        } else if (type == "Statements") {
            bool seenReturn = false;
            for (auto* child : node->children) {
                if (!child) continue;
                if (seenReturn) {
                    reportError(child, "Unreachable statement after return");
                    // Still recurse so the debugger marks the node visited
                    buildSymbolTable(child);
                } else {
                    buildSymbolTable(child);
                    if (child->type == "ReturnStatement") seenReturn = true;
                }
            }
        } else if (type == "ReturnStatement") {
            if (!node->children.empty()) {
                string retType = buildSymbolTable(node->children.front());
                if (!currentMethodRetType.empty()
                    && currentMethodRetType != "unknown"
                    && retType != "unknown"
                    && retType != currentMethodRetType) {
                    reportError(node, "Return type mismatch: expected '"
                        + currentMethodRetType + "', got '" + retType + "'");
                }
            }
        } else if (type == "IfElseStatement") {
            printf("Entering if-else scope\n");
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
        // ──────── NEW CHECK: Undeclared identifiers ────────
        } else if (type == "ID") {
            // This node is an identifier being USED (not declared).
            // Check: has it been declared in any visible scope?
            Record* r = st.lookup(value);
            if (!r) {
                reportError(node, "Undeclared identifier: '" + value + "'");
                return "unknown";
            }
            return r->type;

        } else if (type == "Int") {
            return "int";

        } else if (type == "Float") {
            return "float";

        } else if (type == "Bool") {
            return "boolean";

        } else if (type == "AssignmentStatement") {
            string lhsType = "";
            string rhsType = "";
            int idx = 0;
            for (auto* child : node->children) {
                if (!child) continue;
                string t = buildSymbolTable(child);
                if (idx == 0) lhsType = t;
                else if (idx == 1) rhsType = t;
                idx++;
            }
            if (!lhsType.empty() && !rhsType.empty()
                && lhsType != "unknown" && rhsType != "unknown"
                && lhsType != rhsType) {
                reportError(node, "Type mismatch in assignment: '" + lhsType + "' := '" + rhsType + "'");
            }
        
        // ── Arithmetic: both operands must be same numeric type (int or float) ──
        } else if (type == "AddExpression")  { return checkBinaryOp(node, "+",  "numeric", "");
        } else if (type == "SubExpression")  { return checkBinaryOp(node, "-",  "numeric", "");
        } else if (type == "MultExpression") { return checkBinaryOp(node, "*",  "numeric", "");
        } else if (type == "DivExpression")  { return checkBinaryOp(node, "/",  "numeric", "");
        } else if (type == "PowerExpression"){ return checkBinaryOp(node, "^",  "numeric", "");

        // ── Logical: both operands must be boolean, result is boolean ──
        } else if (type == "AndExpression")  { return checkBinaryOp(node, "&",  "boolean", "boolean");
        } else if (type == "OrExpression")   { return checkBinaryOp(node, "|",  "boolean", "boolean");
        } else if (type == "NegationExpression") { return checkBinaryOp(node, "!", "boolean", "boolean"); 
               
        // ── Comparison: both operands must be int, result is boolean ──
        } else if (type == "LessExpression")  { return checkBinaryOp(node, "<",  "numeric",     "boolean");
        } else if (type == "MoreExpression")  { return checkBinaryOp(node, ">",  "numeric",     "boolean");
        } else if (type == "LessEqExpression"){ return checkBinaryOp(node, "<=", "numeric",     "boolean");
        } else if (type == "MoreEqExpression"){ return checkBinaryOp(node, ">=", "numeric",     "boolean");


        // ── Equality: both operands must match (any type), result is boolean ──
        } else if (type == "EqExpression")    { return checkBinaryOp(node, "=",  "",        "boolean");
        } else if (type == "NotEqExpression") { return checkBinaryOp(node, "!=", "",        "boolean");
        } else if (type == "ArrayExperssion") {
            return checkArrayAccess(node);

        } else if (type == "FunctionCall") {
        // A dot-call (expr.method()) has children[0] as the receiver expression.
        // A bare call (method()) has no children, or children[0] is the argument
        // list node (type "Expression"). Distinguish by the first child's type.
        bool isDotCall = !node->children.empty()
                      && node->children.front()->type != "Expression";
        if (isDotCall) {
            // Evaluate the receiver to determine its class type
            string receiverType = buildSymbolTable(node->children.front());
            if (receiverType == "unknown") return "unknown";

            // Check argument types against parameter types
            auto it = node->children.begin();
            ++it;
            if (it != node->children.end() && (*it)->type == "Expression") {
                checkFunctionArgs(node, *it, receiverType, value);
            }

            string retType = lookupMethodInClass(receiverType, value);
            if (retType.empty()) {
                reportError(node, "Undeclared method '" + value
                                  + "' in class '" + receiverType + "'");
                return "unknown";
            }
            return retType;
        } else {
            // Bare call: check arguments if Expression child exists
            if (!node->children.empty() && node->children.front()->type == "Expression") {
                checkFunctionArgs(node, node->children.front(), currentClassName, value);
            }

            Record* r = st.lookup(value);
            if (!r) {
                reportError(node, "Undeclared method: '" + value + "'");
                return "unknown";
            }
            // Constructor call (e.g. MyClass()): return class name as instance type
            if (r->kind == "class") return r->id;
            return r->type;
        }

        } else if (type == "LengthFunction") {
            if (node->children.empty()) return "unknown";
            string operandType = buildSymbolTable(node->children.front());
            if (operandType == "unknown") return "unknown";
            if (operandType.size() < 2 || operandType.substr(operandType.size() - 2) != "[]") {
                reportError(node, "'.length' applied to non-array type '" + operandType + "'");
                return "unknown";
            }
            return "int";


            
        } else if (type == "Expression") {
            // Expression is an argument list node for FunctionCall.
            // Evaluate all children and return the type of the first child
            // (used when Expression wraps a single sub-expression).
            string firstType = "";
            for (auto* child : node->children) {
                if (!child) continue;
                string t = buildSymbolTable(child);
                if (firstType.empty()) firstType = t;
            }
            return !firstType.empty() ? firstType : "unknown";

        } else {
            // Default pass-through: recurse all children
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
        }
    return "";
    }

private:
    // ── Helpers ──────────────────────────────────────────────────

    // Shallow scan: walk the AST looking for Class nodes and record every
    // method's return type. Called once before buildSymbolTable so that
    // dot-call resolution works even for forward-referenced classes.
    void preScanClasses(Node* node) {
        if (!node) return;
        if (node->type == "Class") {
            const string& className = node->value;
            for (auto* child : node->children) {
                if (!child || child->type != "Methods") continue;
                for (auto* method : child->children) {
                    if (!method || method->type != "Method") continue;
                    string retType = "unknown";
                    for (auto* mc : method->children) {
                        if (mc && (mc->type == "Type" || mc->type == "ArrayType")) {
                            retType = getTypeStr(mc);
                            break;
                        }
                    }
                    classMethods[className][method->value] = retType;

                    // Also collect parameter types
                    vector<string> paramTypes;
                    for (auto* mc : method->children) {
                        if (mc && mc->type == "Params") {
                            for (auto* param : mc->children) {
                                if (param && param->type == "Param" && !param->children.empty())
                                    paramTypes.push_back(getTypeStr(param->children.front()));
                            }
                        }
                    }
                    classMethodParams[className][method->value] = paramTypes;

                    printf("[preScan] class=%s method=%s retType=%s params=%zu\n",
                           className.c_str(), method->value.c_str(), retType.c_str(), paramTypes.size());
                }
            }
            return;  // don't recurse further — we only need the top-level class members
        }
        for (auto* child : node->children)
            if (child) preScanClasses(child);
    }

    // Look up a method's return type in a specific class (uses the pre-scanned map).
    // Returns "" if the class or method is not found.
    string lookupMethodInClass(const string& className, const string& methodName) const {
        auto cit = classMethods.find(className);
        if (cit == classMethods.end()) return "";
        auto mit = cit->second.find(methodName);
        if (mit == cit->second.end()) return "";
        return mit->second;
    }

    // Look up a method's parameter types in a specific class.
    vector<string> lookupMethodParamsInClass(const string& className, const string& methodName) const {
        auto cit = classMethodParams.find(className);
        if (cit == classMethodParams.end()) return {};
        auto mit = cit->second.find(methodName);
        if (mit == cit->second.end()) return {};
        return mit->second;
    }

    // Check that argument types in an Expression node match the expected parameter types.
    void checkFunctionArgs(Node* callNode, Node* exprNode,
                           const string& className, const string& methodName) {
        vector<string> expectedParams = lookupMethodParamsInClass(className, methodName);
        if (expectedParams.empty() && classMethodParams.count(className)
            && classMethodParams.at(className).count(methodName)) {
            // Method exists but has zero params — still check arg count
            expectedParams = {};
        }

        // Collect argument types from Expression children
        vector<string> argTypes;
        for (auto* arg : exprNode->children) {
            if (!arg) continue;
            argTypes.push_back(buildSymbolTable(arg));
        }

        // If we couldn't find the method params, skip the check
        if (!classMethodParams.count(className)
            || !classMethodParams.at(className).count(methodName)) return;

        // Check argument count
        if (argTypes.size() != expectedParams.size()) {
            reportError(callNode, "Wrong number of arguments for '" + methodName
                + "': expected " + to_string(expectedParams.size())
                + ", got " + to_string(argTypes.size()));
            return;
        }

        // Check each argument type
        for (size_t i = 0; i < argTypes.size(); i++) {
            if (argTypes[i] == "unknown" || expectedParams[i] == "unknown") continue;
            if (argTypes[i] != expectedParams[i]) {
                reportError(callNode, "Argument type mismatch for '" + methodName
                    + "': parameter " + to_string(i + 1)
                    + " expected '" + expectedParams[i]
                    + "', got '" + argTypes[i] + "'");
            }
        }
    }

    // Returns the type string for a "Type" or "ArrayType" node.
    static string getTypeStr(const Node* node) {
        if (node->type == "ArrayType" && !node->children.empty())
            return node->children.front()->value + "[]";
        return node->value;
    }

    // Evaluate both children, enforce type rules, return result type.
    // requiredType: both operands must be this type. Empty = they just must match each other.
    // resultType:   the type this expression produces (e.g. "boolean" for comparisons).
    string checkBinaryOp(Node* node, const string& op,
                         const string& requiredType, const string& resultType) {
        string lhsType = "", rhsType = "";
        int idx = 0;
        for (auto* child : node->children) {
            if (!child) continue;
            string t = buildSymbolTable(child);
            if (idx == 0) lhsType = t;
            else if (idx == 1) rhsType = t;
            idx++;
        }
        if (lhsType == "unknown" || rhsType == "unknown") return "unknown";
        bool valid;
        string effectiveResult = resultType;
        if (requiredType == "numeric") {
            // both must be the same type AND numeric (int or float)
            bool isNumeric = (lhsType == "int" || lhsType == "float");
            valid = isNumeric && (lhsType == rhsType);
            if (resultType.empty())
                effectiveResult = lhsType; // int+int→int, float+float→float
        } else if (requiredType.empty()) {
            valid = (lhsType == rhsType);   // equality ops: any matching type
        } else {
            valid = (lhsType == requiredType && rhsType == requiredType); // typed ops
        }
        if (!valid) {
            reportError(node, "invalid operand type for '" + op + "': "
                              + lhsType + " " + op + " " + rhsType);
            return "unknown";
        }
        return effectiveResult;
    }


    string checkArrayAccess(Node* node) {
        if (node->children.size() < 2) return "unknown";

        auto it = node->children.begin();
        Node* arrChild = *it;
        ++it;
        Node* idxChild = *it;

        string arrType = buildSymbolTable(arrChild);  // children[0]: array
        string idxType = buildSymbolTable(idxChild);  // children[1]: index

        // Function calls are not valid as array indices — even if they return int.
        // An index must be a plain integer literal or variable.
        if (idxChild->type == "FunctionCall") {
            reportError(node, "Function call not valid as array index: '" + idxChild->value + "()'");
            return "unknown";
        }

        if (arrType == "unknown" || idxType == "unknown") return "unknown";

        if (idxType != "int") {
            reportError(node, "Array index must be 'int', got '" + idxType + "'");
            return "unknown";
        }

        if (arrType.size() < 2 || arrType.substr(arrType.size() - 2) != "[]") {
            reportError(node, "Subscript applied to non-array type '" + arrType + "'");
            return "unknown";
        }

        return arrType.substr(0, arrType.size() - 2); // strip "[]" → element type
    }

    // Returns true if node (or any descendant) is a ReturnStatement.
    static bool hasReturn(Node* node) {
        if (!node) return false;
        if (node->type == "ReturnStatement") return true;
        for (auto* child : node->children)
            if (hasReturn(child)) return true;
        return false;
    }

    // Tag the AST node with the error and print it.
    void reportError(Node* node, const string& msg) {
        errors++;
        node->errorMsg = msg;
        cerr << "\t@error at line " << node->lineno << ". " << msg << endl;
    }
};

#endif
