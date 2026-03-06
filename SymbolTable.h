#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>

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

void generate_dot_content(int& count, ofstream& out) const {
    int myId = count++;

    // Build a human-readable title from the scope label
    string title;
    if (label == "global")
        title = "Global";
    else if (label.size() > 6 && label.substr(0, 6) == "class:")
        title = "Class (" + label.substr(6) + ")";
    else if (label.size() > 7 && label.substr(0, 7) == "method:")
        title = "Method (" + label.substr(7) + ")";
    else if (label == "main")
        title = "Main";
    else
        title = "Inner Scope(" + label + ")";

    // Abbreviate kind to match the style in the reference image
    auto kindShort = [](const string& k) -> string {
        if (k == "variable" || k == "parameter") return "var";
        if (k == "method")  return "method";
        return k;
    };

    // Outer wrapper: title in red above the data grid
    out << "  s" << myId
        << " [shape=none, margin=0, label=<" << endl;
    out << "    <TABLE BORDER=\"0\" CELLBORDER=\"0\" CELLSPACING=\"6\" CELLPADDING=\"0\">" << endl;
    out << "      <TR><TD ALIGN=\"CENTER\"><FONT COLOR=\"red\"><B>"
        << title << "</B></FONT></TD></TR>" << endl;
    out << "      <TR><TD>" << endl;
    // Inner data grid
    out << "        <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"6\">" << endl;
    if (records.empty()) {
        out << "          <TR><TD COLSPAN=\"3\"><I>(empty)</I></TD></TR>" << endl;
    } else {
        for (const auto& kv : records) {
            const Record* r = kv.second;
            out << "          <TR>"
                << "<TD ALIGN=\"LEFT\">" << r->id << "</TD>"
                << "<TD ALIGN=\"LEFT\">" << kindShort(r->kind) << "</TD>"
                << "<TD ALIGN=\"LEFT\">" << r->type << "</TD>"
                << "</TR>" << endl;
        }
    }
    out << "        </TABLE>" << endl;
    out << "      </TD></TR>" << endl;
    out << "    </TABLE>>];" << endl;

    // Recurse into child scopes, draw plain edges
    for (auto* child : children) {
        int childId = count;
        child->generate_dot_content(count, out);
        out << "  s" << myId << " -> s" << childId << ";" << endl;
    }
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

void generate_dot() const {
    ofstream out("symtable.dot");
    out << "digraph SymbolTable {" << endl;
    out << "  rankdir=TB;" << endl;
    out << "  node [fontname=\"Helvetica\", fontsize=12];" << endl;
    out << "  edge [color=black];" << endl;
    int count = 0;
    root->generate_dot_content(count, out);
    out << "}" << endl;
    out.close();
    printf("\nBuilt symbol table at symtable.dot. Run 'make symtable' to generate the PDF.\n");
}

    void resetTable() {
        root->resetScope();
        current = root;
    }
};

#endif
