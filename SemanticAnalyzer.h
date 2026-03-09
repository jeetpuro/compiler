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

    // ── Entry point ──────────────────────────────────────────────

    void analyze(Node* root) {
        if (!root) return;
        buildSymbolTable(root);
        st.generate_dot();
    }

    // ── Symbol Table Construction + Semantic Checks ──────────────

    string buildSymbolTable(Node* node) {
        if (!node) return "";

        const string& type  = node->type;
        const string& value = node->value; // kan döpas om till name

        if (type == "Program") {
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);

        } else if (type == "Class") {
            // Register class name in the current (global) scope
            if (!st.put(value, new Record(value, "class", "class"))) {
                reportError(node, "Already Declared Class: '" + value + "'");
            }
            st.enterScope("class:" + value);
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
            st.exitScope();

        } else if (type == "Method") {
            // Find return type node (first Type or ArrayType child)
            string retType = "unknown";
            for (auto* child : node->children) {
                if (child && (child->type == "Type" || child->type == "ArrayType")) {
                    retType = getTypeStr(child);
                    break;
                }
            }
            // Register method in the class scope (before entering method scope)
            if (!st.put(value, new Record(value, "method", retType))) {
                reportError(node, "Already Declared Function: '" + value + "'");
            }
            st.enterScope("method:" + value);
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
            st.exitScope();

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
            // No need to recurse: children are just a Type node

        } else if (type == "MainStatement") {
            st.enterScope("main");
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
            st.exitScope();

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
        // Recurse children so undeclared-identifier checks still run on arguments
        for (auto* child : node->children)
            if (child) buildSymbolTable(child);
        Record* r = st.lookup(value);
        return r ? r->type : "unknown";



        } else {
            // Default pass-through: recurse all children
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
        }
    return "";
    }

private:
    // ── Helpers ──────────────────────────────────────────────────

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
        string arrType = buildSymbolTable(*it);   // children[0]: array
        ++it;
        string idxType = buildSymbolTable(*it);   // children[1]: index

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

    // Tag the AST node with the error and print it.
    void reportError(Node* node, const string& msg) {
        errors++;
        node->errorMsg = msg;
        cerr << "\t@error at line " << node->lineno << ". " << msg << endl;
    }
};

#endif
