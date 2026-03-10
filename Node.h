#ifndef NODE_H
#define	NODE_H

#include <list>
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;


class Node {
public:
	int id, lineno;
	string type, value;
	string errorMsg = "";   // set by buildSymbolTable when a semantic error is found
	bool visited = false;    // set by SemanticAnalyzer when this node is checked
	list<Node*> children;
	Node(string t, string v, int l) : type(t), value(v), lineno(l){}
	Node()
	{
		type = "uninitialised";
		value = "uninitialised"; }   // Bison needs this.
  
	void print_tree(int depth=0) {
		for(int i=0; i<depth; i++)
		cout << "  ";
		cout << type << ":" << value << endl; //<< " @line: "<< lineno << endl;
		for(auto i=children.begin(); i!=children.end(); i++)
		(*i)->print_tree(depth+1);
	}
  
	void generate_tree() {
		std::ofstream outStream;
		char* filename = "tree.dot";
	  	outStream.open(filename);

		int count = 0;
		outStream << "digraph {" << std::endl;
		generate_tree_content(count, &outStream);
		outStream << "}" << std::endl;
		outStream.close();

		printf("\nBuilt a parse-tree at %s. Use 'make tree' to generate the pdf version.\n", filename);
  	}

  	void generate_tree_content(int &count, ofstream *outStream) {
	  id = count++;
	  *outStream << "n" << id << " [label=\"" << type << ":" << value << "\"];" << endl;

	  for (auto i = children.begin(); i != children.end(); i++)
	  {
		  (*i)->generate_tree_content(count, outStream);
		  *outStream << "n" << id << " -> n" << (*i)->id << endl;
	  }
  }

	// ── Visual Semantic Debugger ───────────────────────────────────────────

	void generate_tree_semantic() {
		std::ofstream outStream;
		const char* filename = "tree_semantic.dot";
		outStream.open(filename);
		int count = 0;
		outStream << "digraph {" << std::endl;
		outStream << "  node [style=filled, fillcolor=white, shape=box];" << std::endl;
		generate_tree_semantic_content(count, &outStream);
		outStream << "}" << std::endl;
		outStream.close();
		printf("\nBuilt semantic tree at %s. Run 'make tree_semantic' to generate the PDF.\n", filename);
	}

	void generate_tree_semantic_content(int& count, ofstream* out) {
		id = count++;

		// Escape a string for use inside a DOT double-quoted label
		auto esc = [](const string& s) {
			string r;
			for (char c : s) {
				if      (c == '"')  r += "\\\"";
				else if (c == '\\') r += "\\\\";
				else if (c == '\n') r += "\\n";
				else r += c;
			}
			return r;
		};

		string lbl = esc(type + ":" + value);

		if (!errorMsg.empty()) {
			// Red double-octagon — semantic error node
			*out << "n" << id
			     << " [label=\"" << lbl << "\\n" << esc(errorMsg) << "\""
			     << ", style=filled, fillcolor=red, fontcolor=white"
			     << ", shape=doubleoctagon];" << endl;
		} else if (visited) {
			// Green — semantically checked node
			*out << "n" << id
			     << " [label=\"" << lbl << "\""
			     << ", style=filled, fillcolor=\"#90EE90\"];" << endl;
		} else {
			*out << "n" << id
			     << " [label=\"" << lbl << "\"];" << endl;
		}

		for (auto* child : children) {
			child->generate_tree_semantic_content(count, out);
			*out << "n" << id << " -> n" << child->id << endl;
		}
	}

};

#endif