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
    }

    // ── Symbol Table Construction + Semantic Checks ──────────────

    void buildSymbolTable(Node* node) {
        if (!node) return;

        const string& type  = node->type;
        const string& value = node->value;

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
            if (!st.lookup(value)) {
                reportError(node, "Undeclared identifier: '" + value + "'");
                
            }

        } else {
            // Default pass-through: recurse all children
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
        }
    }

private:
    // ── Helpers ──────────────────────────────────────────────────

    // Returns the type string for a "Type" or "ArrayType" node.
    static string getTypeStr(const Node* node) {
        if (node->type == "ArrayType" && !node->children.empty())
            return node->children.front()->value + "[]";
        return node->value;
    }

    // Tag the AST node with the error and print it.
    void reportError(Node* node, const string& msg) {
        errors++;
        node->errorMsg = msg;
        cerr << "\t@error at line " << node->lineno << ". " << msg << endl;
    }
};

#endif
