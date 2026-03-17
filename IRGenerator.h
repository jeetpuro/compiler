#ifndef IRGENERATOR_H
#define IRGENERATOR_H

#include "CFGDotWriter.h"
#include "IR.h"
#include "Node.h"
#include <set>
#include <string>
#include <vector>

class IRGenerator {
public:
	ProgramIR generate(Node* root);
	void printTAC(const ProgramIR& ir);
	void writeCFGDot(const ProgramIR& ir, const std::string& filename);

private:
	ProgramIR program;
	int tempCounter = 0;
	int blockCounter = 0;
	FunctionIR* currentFunc = nullptr;
	int currentBlockId = -1;
	std::string currentClassName;
	std::set<std::string> classNames;
	std::vector<int> breakTargets;
	std::vector<int> continueTargets;

	std::string newTemp();
	int newBlock(const std::string& name);
	BasicBlock& blockById(int id);
	BasicBlock& currentBlock();
	void emit(IROp op,
			  const std::string& dst = "",
			  const std::string& src1 = "",
			  const std::string& src2 = "",
			  const std::string& extra = "",
			  int line = 0);
	void addEdge(int fromId, int toId);
	bool blockTerminates(int blockId) const;

	void collectClassNames(Node* root);
	void genProgram(Node* root);
	void genClass(Node* classNode);
	void genMethod(Node* methodNode, const std::string& ownerClass);
	void genMain(Node* mainNode);
	void genStmt(Node* stmt);
	std::string genExpr(Node* expr);
};

#endif
