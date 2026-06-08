#ifndef IR_H
#define IR_H

#include <map>
#include <set>
#include <string>
#include <vector>

enum class IROp {
	Assign,
	Add,
	Sub,
	Mul,
	Div,
	Pow,
	CmpLT,
	CmpLE,
	CmpGT,
	CmpGE,
	CmpEQ,
	CmpNE,
	And,
	Or,
	Not,
	Goto,
	IfFalseGoto,
	Param,
	Call,
	Return,
	Print,
	Read,
	ArrayLoad,
	ArrayStore,
	Length,
	NewObject,
	NewArray
};

struct TAC {
	IROp op = IROp::Assign;
	std::string dst;
	std::string src1;
	std::string src2;
	std::string extra;
	int line = 0;

	TAC() = default;

	TAC(IROp op,
		const std::string& dst,
		const std::string& src1 = "",
		const std::string& src2 = "",
		const std::string& extra = "",
		int line = 0)
		: op(op), dst(dst), src1(src1), src2(src2), extra(extra), line(line) {}
};

struct BasicBlock {
	int id = -1;
	std::string name;
	std::vector<TAC> code;
	std::vector<int> succ;

	void addSuccessor(int nextId) {
		for (int existing : succ) {
			if (existing == nextId) {
				return;
			}
		}
		succ.push_back(nextId);
	}
};

struct FunctionIR {
	std::string name;
	std::vector<std::string> params;
	std::vector<BasicBlock> blocks;
	int entryBlock = -1;
};

struct ProgramIR {
	std::map<std::string, FunctionIR> functions;
	std::map<std::string, std::set<std::string>> classFields;       // class -> scalar field names
	std::map<std::string, std::set<std::string>> classArrayFields;  // class -> array field names
};

#endif
