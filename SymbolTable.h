#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#include <string>
#include <map>
#include <vector>
#include <iostream>

using namespace std;

// ─────────────────────────────────────────────
// Record: one symbol entry in the table
// ─────────────────────────────────────────────
class Record {
public:
    string id;
    string kind;   // "class" | "method" | "variable" | "parameter"
    string type;

    Record(const string& id, const string& kind, const string& type)
        : id(id), kind(kind), type(type) {}

    void printRecord(int depth) const {
        string indent(depth * 2, ' ');
        cout << indent << "[" << kind << "] " << id << " : " << type << endl;
    }
};

// ─────────────────────────────────────────────
// Scope: one node in the scope tree
// ─────────────────────────────────────────────
class Scope {
public:
    string label;                   // e.g. "class:Foo", "method:bar", "main"
    Scope* parent;
    vector<Scope*> children;
    map<string, Record*> records;   // symbols declared in this scope
    int next = 0;                   // child-visit index for tree traversal

    explicit Scope(Scope* parent = nullptr) : parent(parent) {}

    ~Scope() {
        for (auto& kv : records) delete kv.second;
        for (auto* child : children) delete child;
    }

    // Check only THIS scope (no parent walk) — for duplicate detection
    bool containsKey(const string& key) const {
        return records.find(key) != records.end();
    }

    // Walk up the parent chain — for use-site lookup (Part 2)
    Record* lookup(const string& key) const {
        auto it = records.find(key);
        if (it != records.end()) return it->second;
        if (parent) return parent->lookup(key);
        return nullptr;
    }

    // Return the next child scope, creating it on demand
    Scope* nextChild() {
        if (next >= (int)children.size())
            children.push_back(new Scope(this));
        return children[next++];
    }

    void resetScope() {
        next = 0;
        for (auto* child : children) child->resetScope();
    }

    void printScope(int depth) const {
        string indent(depth * 2, ' ');
        if (!label.empty())
            cout << indent << "--- Scope [" << label << "] ---" << endl;
        for (const auto& kv : records)
            kv.second->printRecord(depth + 1);
        for (auto* child : children)
            child->printScope(depth + 1);
    }
};

// ─────────────────────────────────────────────
// SymbolTable: manages root + current scope
// ─────────────────────────────────────────────
class SymbolTable {
public:
    Scope* root;
    Scope* current;

    SymbolTable() {
        root = new Scope(nullptr);
        root->label = "global";
        current = root;
    }

    ~SymbolTable() { delete root; }

    void enterScope(const string& label = "") {
        current = current->nextChild();
        current->label = label;
    }

    void exitScope() {
        if (current->parent)
            current = current->parent;
    }

    // Returns true on success, false if key already declared in current scope
    bool put(const string& key, Record* record) {
        if (current->containsKey(key)) {
            delete record;
            return false;
        }
        current->records[key] = record;
        return true;
    }

    Record* lookup(const string& key) const {
        return current->lookup(key);
    }

    void printTable() const {
        cout << "\n=== Symbol Table ===" << endl;
        root->printScope(0);
        cout << "====================" << endl;
    }

    void resetTable() {
        root->resetScope();
        current = root;
    }
};

#endif
