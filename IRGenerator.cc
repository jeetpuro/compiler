#include "IRGenerator.h"

#include <cctype>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace {

Node* firstChildOfType(Node* node, const string& wantedType) {
	if (!node) {
		return nullptr;
	}
	for (Node* child : node->children) {
		if (child && child->type == wantedType) {
			return child;
		}
	}
	return nullptr;
}

string quoteString(const string& value) {
	string result = "\"";
	for (char ch : value) {
		if (ch == '\\' || ch == '"') {
			result += '\\';
		}
		result += ch;
	}
	result += "\"";
	return result;
}

bool isBinaryNode(const string& type) {
	return type == "AddExpression" || type == "SubExpression" ||
		   type == "MultExpression" || type == "DivExpression" ||
		   type == "PowerExpression" || type == "LessExpression" ||
		   type == "LessEqExpression" || type == "MoreExpression" ||
		   type == "MoreEqExpression" || type == "EqExpression" ||
		   type == "NotEqExpression" || type == "AndExpression" ||
		   type == "OrExpression";
}

IROp opForNode(const string& type) {
	if (type == "AddExpression") return IROp::Add;
	if (type == "SubExpression") return IROp::Sub;
	if (type == "MultExpression") return IROp::Mul;
	if (type == "DivExpression") return IROp::Div;
	if (type == "PowerExpression") return IROp::Pow;
	if (type == "LessExpression") return IROp::CmpLT;
	if (type == "LessEqExpression") return IROp::CmpLE;
	if (type == "MoreExpression") return IROp::CmpGT;
	if (type == "MoreEqExpression") return IROp::CmpGE;
	if (type == "EqExpression") return IROp::CmpEQ;
	if (type == "NotEqExpression") return IROp::CmpNE;
	if (type == "AndExpression") return IROp::And;
	return IROp::Or;
}

string toFunctionLabel(const string& ownerClass, const string& name) {
	return ownerClass.empty() ? name : ownerClass + "_" + name;
}

} // namespace

ProgramIR IRGenerator::generate(Node* root) {
	program.functions.clear();
	classNames.clear();
	tempCounter = 0;
	blockCounter = 0;
	currentFunc = nullptr;
	currentBlockId = -1;
	currentClassName.clear();
	breakTargets.clear();
	continueTargets.clear();

	collectClassNames(root);
	genProgram(root);
	return program;
}

void IRGenerator::printTAC(const ProgramIR& ir) {
	for (const auto& kv : ir.functions) {
		const FunctionIR& function = kv.second;
		cout << "\n=== TAC for " << function.name << " ===" << endl;
		for (const BasicBlock& block : function.blocks) {
			cout << "B" << block.id << " (" << block.name << "):" << endl;
			for (const TAC& tac : block.code) {
				cout << "  " << CFGDotWriter::tacToString(tac) << endl;
			}
			if (block.code.empty()) {
				cout << "  <empty>" << endl;
			}
		}
	}
}

void IRGenerator::writeCFGDot(const ProgramIR& ir, const string& filename) {
	CFGDotWriter::write(ir, filename);
}

string IRGenerator::newTemp() {
	return "t" + to_string(tempCounter++);
}

int IRGenerator::newBlock(const string& name) {
	if (!currentFunc) {
		throw runtime_error("newBlock called without an active function");
	}

	BasicBlock block;
	block.id = blockCounter++;
	block.name = name;
	currentFunc->blocks.push_back(block);
	return block.id;
}

BasicBlock& IRGenerator::blockById(int id) {
	for (BasicBlock& block : currentFunc->blocks) {
		if (block.id == id) {
			return block;
		}
	}
	throw runtime_error("Unknown basic block id: " + to_string(id));
}

BasicBlock& IRGenerator::currentBlock() {
	return blockById(currentBlockId);
}

void IRGenerator::emit(IROp op,
					   const string& dst,
					   const string& src1,
					   const string& src2,
					   const string& extra,
					   int line) {
	currentBlock().code.push_back(TAC(op, dst, src1, src2, extra, line));
}

void IRGenerator::addEdge(int fromId, int toId) {
	blockById(fromId).addSuccessor(toId);
}

bool IRGenerator::blockTerminates(int blockId) const {
	if (!currentFunc) {
		return false;
	}

	for (const BasicBlock& block : currentFunc->blocks) {
		if (block.id != blockId) {
			continue;
		}
		if (block.code.empty()) {
			return false;
		}
		IROp op = block.code.back().op;
		return op == IROp::Goto || op == IROp::IfFalseGoto || op == IROp::Return;
	}

	return false;
}

void IRGenerator::collectClassNames(Node* root) {
	if (!root || root->type != "Program") {
		return;
	}

	for (Node* child : root->children) {
		if (!child) {
			continue;
		}
		if (child->type == "Class") {
			classNames.insert(child->value);
		} else if (child->type == "Classes") {
			for (Node* cls : child->children) {
				if (cls && cls->type == "Class") {
					classNames.insert(cls->value);
				}
			}
		}
	}
}

void IRGenerator::genProgram(Node* root) {
	if (!root || root->type != "Program") {
		return;
	}

	for (Node* child : root->children) {
		if (!child) {
			continue;
		}

		if (child->type == "Classes") {
			for (Node* cls : child->children) {
				if (cls && cls->type == "Class") {
					genClass(cls);
				}
			}
		} else if (child->type == "Class") {
			genClass(child);
		} else if (child->type == "MainStatement") {
			genMain(child);
		}
	}
}

void IRGenerator::genClass(Node* classNode) {
	if (!classNode) {
		return;
	}

	string savedClass = currentClassName;
	currentClassName = classNode->value;

	for (Node* child : classNode->children) {
		if (!child || child->type != "Methods") {
			continue;
		}
		for (Node* method : child->children) {
			if (method && method->type == "Method") {
				genMethod(method, currentClassName);
			}
		}
	}

	currentClassName = savedClass;
}

void IRGenerator::genMethod(Node* methodNode, const string& ownerClass) {
	if (!methodNode) {
		return;
	}

	string functionName = toFunctionLabel(ownerClass, methodNode->value);
	FunctionIR& function = program.functions[functionName];
	function = FunctionIR();
	function.name = functionName;

	currentFunc = &function;
	currentBlockId = newBlock(functionName + "_entry");
	function.entryBlock = currentBlockId;

	Node* body = nullptr;
	for (Node* child : methodNode->children) {
		if (!child) {
			continue;
		}
		if (child->type == "Params") {
			for (Node* param : child->children) {
				if (param && param->type == "Param") {
					function.params.push_back(param->value);
				}
			}
		} else if (child->type == "Statements") {
			body = child;
		}
	}

	genStmt(body);
}

void IRGenerator::genMain(Node* mainNode) {
	FunctionIR& function = program.functions["main"];
	function = FunctionIR();
	function.name = "main";

	currentFunc = &function;
	currentBlockId = newBlock("main_entry");
	function.entryBlock = currentBlockId;

	for (Node* child : mainNode->children) {
		if (child && child->type == "Statements") {
			genStmt(child);
		}
	}
}

void IRGenerator::genStmt(Node* stmt) {
	if (!stmt) {
		return;
	}

	if (stmt->type == "Statements") {
		for (Node* child : stmt->children) {
			genStmt(child);
		}
		return;
	}

	if (stmt->type == "VarDecl" && stmt->value.empty()) {
		for (Node* child : stmt->children) {
			genStmt(child);
		}
		return;
	}

	if (stmt->type == "var1") {
		Node* child = firstChildOfType(stmt, "VarDecl");
		if (!child && !stmt->children.empty()) {
			child = stmt->children.front();
		}
		genStmt(child);
		return;
	}

	if (stmt->type == "Initialization") {
		auto it = stmt->children.begin();
		Node* lhs = (it != stmt->children.end()) ? *it : nullptr;
		Node* rhs = nullptr;
		if (it != stmt->children.end()) {
			++it;
			rhs = (it != stmt->children.end()) ? *it : nullptr;
		}
		if (lhs && rhs) {
			string rhsValue = genExpr(rhs);
			if (lhs->type == "ID") {
				emit(IROp::Assign, lhs->value, rhsValue, "", "", stmt->lineno);
			}
		}
		return;
	}

	if (stmt->type == "VarDecl") {
		Node* initExpr = nullptr;
		for (Node* child : stmt->children) {
			if (!child) {
				continue;
			}
			if (child->type == "Keyword" || child->type == "Type" || child->type == "ArrayType") {
				continue;
			}
			initExpr = child;
		}
		if (initExpr) {
			string rhsValue = genExpr(initExpr);
			emit(IROp::Assign, stmt->value, rhsValue, "", "", stmt->lineno);
		}
		return;
	}

	if (stmt->type == "AssignmentStatement") {
		auto it = stmt->children.begin();
		Node* lhs = (it != stmt->children.end()) ? *it : nullptr;
		Node* rhs = nullptr;
		if (it != stmt->children.end()) {
			++it;
			rhs = (it != stmt->children.end()) ? *it : nullptr;
		}
		if (!lhs || !rhs) {
			return;
		}

		string rhsValue = genExpr(rhs);
		if (lhs->type == "ID") {
			emit(IROp::Assign, lhs->value, rhsValue, "", "", stmt->lineno);
		} else if (lhs->type == "ArrayExperssion") {
			auto lhsIt = lhs->children.begin();
			Node* base = (lhsIt != lhs->children.end()) ? *lhsIt : nullptr;
			Node* index = nullptr;
			if (lhsIt != lhs->children.end()) {
				++lhsIt;
				index = (lhsIt != lhs->children.end()) ? *lhsIt : nullptr;
			}
			if (base && index) {
				emit(IROp::ArrayStore,
					 genExpr(base),
					 genExpr(index),
					 rhsValue,
					 "",
					 stmt->lineno);
			}
		}
		return;
	}

	if (stmt->type == "PrintStatement") {
		if (!stmt->children.empty()) {
			emit(IROp::Print, "", genExpr(stmt->children.front()), "", "", stmt->lineno);
		}
		return;
	}

	if (stmt->type == "readStatement") {
		if (!stmt->children.empty()) {
			Node* target = stmt->children.front();
			if (target && target->type == "ID") {
				emit(IROp::Read, target->value, "", "", "", stmt->lineno);
			}
		}
		return;
	}

	if (stmt->type == "ReturnStatement") {
		if (!stmt->children.empty()) {
			emit(IROp::Return, "", genExpr(stmt->children.front()), "", "", stmt->lineno);
		} else {
			emit(IROp::Return, "", "", "", "", stmt->lineno);
		}
		return;
	}

	if (stmt->type == "BreakStatement") {
		if (!breakTargets.empty()) {
			int target = breakTargets.back();
			emit(IROp::Goto, "", "", "", to_string(target), stmt->lineno);
			addEdge(currentBlockId, target);
		}
		return;
	}

	if (stmt->type == "ContinueStatement") {
		if (!continueTargets.empty()) {
			int target = continueTargets.back();
			emit(IROp::Goto, "", "", "", to_string(target), stmt->lineno);
			addEdge(currentBlockId, target);
		}
		return;
	}

	if (stmt->type == "IfStatement") {
		auto it = stmt->children.begin();
		Node* cond = (it != stmt->children.end()) ? *it : nullptr;
		Node* thenBody = nullptr;
		if (it != stmt->children.end()) {
			++it;
			thenBody = (it != stmt->children.end()) ? *it : nullptr;
		}
		if (!cond || !thenBody) {
			return;
		}

		int origin = currentBlockId;
		string condValue = genExpr(cond);
		int thenBlockId = newBlock("if_then");
		int joinBlockId = newBlock("if_join");

		emit(IROp::IfFalseGoto, "", condValue, "", to_string(joinBlockId), stmt->lineno);
		addEdge(origin, thenBlockId);
		addEdge(origin, joinBlockId);

		currentBlockId = thenBlockId;
		genStmt(thenBody);
		if (!blockTerminates(currentBlockId)) {
			emit(IROp::Goto, "", "", "", to_string(joinBlockId), stmt->lineno);
			addEdge(currentBlockId, joinBlockId);
		}

		currentBlockId = joinBlockId;
		return;
	}

	if (stmt->type == "IfElseStatement") {
		auto it = stmt->children.begin();
		Node* cond = (it != stmt->children.end()) ? *it : nullptr;
		Node* thenBody = nullptr;
		Node* elseBody = nullptr;
		if (it != stmt->children.end()) {
			++it;
			thenBody = (it != stmt->children.end()) ? *it : nullptr;
		}
		if (it != stmt->children.end()) {
			++it;
			elseBody = (it != stmt->children.end()) ? *it : nullptr;
		}
		if (!cond || !thenBody || !elseBody) {
			return;
		}

		int origin = currentBlockId;
		string condValue = genExpr(cond);
		int thenBlockId = newBlock("if_then");
		int elseBlockId = newBlock("if_else");
		int joinBlockId = newBlock("if_join");

		emit(IROp::IfFalseGoto, "", condValue, "", to_string(elseBlockId), stmt->lineno);
		addEdge(origin, thenBlockId);
		addEdge(origin, elseBlockId);

		currentBlockId = thenBlockId;
		genStmt(thenBody);
		if (!blockTerminates(currentBlockId)) {
			emit(IROp::Goto, "", "", "", to_string(joinBlockId), stmt->lineno);
			addEdge(currentBlockId, joinBlockId);
		}

		currentBlockId = elseBlockId;
		genStmt(elseBody);
		if (!blockTerminates(currentBlockId)) {
			emit(IROp::Goto, "", "", "", to_string(joinBlockId), stmt->lineno);
			addEdge(currentBlockId, joinBlockId);
		}

		currentBlockId = joinBlockId;
		return;
	}

	if (stmt->type == "ForStatement") {
		auto it = stmt->children.begin();
		Node* init = (it != stmt->children.end()) ? *it : nullptr;
		Node* cond = nullptr;
		Node* update = nullptr;
		Node* body = nullptr;
		if (it != stmt->children.end()) {
			++it;
			cond = (it != stmt->children.end()) ? *it : nullptr;
		}
		if (it != stmt->children.end()) {
			++it;
			update = (it != stmt->children.end()) ? *it : nullptr;
		}
		if (it != stmt->children.end()) {
			++it;
			body = (it != stmt->children.end()) ? *it : nullptr;
		}

		if (init && init->type != "EmptyInit") {
			genStmt(init);
		}

		int origin = currentBlockId;
		int condBlockId = newBlock("for_cond");
		int bodyBlockId = newBlock("for_body");
		int stepBlockId = newBlock("for_step");
		int exitBlockId = newBlock("for_exit");

		emit(IROp::Goto, "", "", "", to_string(condBlockId), stmt->lineno);
		addEdge(origin, condBlockId);

		currentBlockId = condBlockId;
		if (cond && cond->type != "EmptyCond") {
			Node* condExpr = cond;
			if (cond->type == "Condition" && !cond->children.empty()) {
				condExpr = cond->children.front();
			}
			string condValue = genExpr(condExpr);
			emit(IROp::IfFalseGoto, "", condValue, "", to_string(exitBlockId), stmt->lineno);
			addEdge(condBlockId, bodyBlockId);
			addEdge(condBlockId, exitBlockId);
		} else {
			emit(IROp::Goto, "", "", "", to_string(bodyBlockId), stmt->lineno);
			addEdge(condBlockId, bodyBlockId);
		}

		breakTargets.push_back(exitBlockId);
		continueTargets.push_back(stepBlockId);

		currentBlockId = bodyBlockId;
		genStmt(body);
		if (!blockTerminates(currentBlockId)) {
			emit(IROp::Goto, "", "", "", to_string(stepBlockId), stmt->lineno);
			addEdge(currentBlockId, stepBlockId);
		}

		currentBlockId = stepBlockId;
		if (update) {
			genStmt(update);
		}
		if (!blockTerminates(currentBlockId)) {
			emit(IROp::Goto, "", "", "", to_string(condBlockId), stmt->lineno);
			addEdge(currentBlockId, condBlockId);
		}

		continueTargets.pop_back();
		breakTargets.pop_back();

		currentBlockId = exitBlockId;
		return;
	}

	if (stmt->type == "Condition") {
		if (!stmt->children.empty()) {
			genExpr(stmt->children.front());
		}
		return;
	}

	if (stmt->type == "FunctionCall") {
		genExpr(stmt);
		return;
	}

	if (stmt->type == "ID" || stmt->type == "ArrayExperssion" || isBinaryNode(stmt->type)) {
		genExpr(stmt);
	}
}

string IRGenerator::genExpr(Node* expr) {
	if (!expr) {
		return "";
	}

	if (expr->type == "ID" || expr->type == "Int" || expr->type == "Float" || expr->type == "Bool") {
		return expr->value;
	}

	if (expr->type == "String") {
		return quoteString(expr->value);
	}

	if (isBinaryNode(expr->type)) {
		auto it = expr->children.begin();
		Node* left = (it != expr->children.end()) ? *it : nullptr;
		Node* right = nullptr;
		if (it != expr->children.end()) {
			++it;
			right = (it != expr->children.end()) ? *it : nullptr;
		}
		string lhs = genExpr(left);
		string rhs = genExpr(right);
		string temp = newTemp();
		emit(opForNode(expr->type), temp, lhs, rhs, "", expr->lineno);
		return temp;
	}

	if (expr->type == "NegationExpression") {
		if (expr->children.empty()) {
			return "";
		}
		string operand = genExpr(expr->children.front());
		string temp = newTemp();
		emit(IROp::Not, temp, operand, "", "", expr->lineno);
		return temp;
	}

	if (expr->type == "ArrayExperssion") {
		auto it = expr->children.begin();
		Node* base = (it != expr->children.end()) ? *it : nullptr;
		Node* index = nullptr;
		if (it != expr->children.end()) {
			++it;
			index = (it != expr->children.end()) ? *it : nullptr;
		}
		string temp = newTemp();
		emit(IROp::ArrayLoad, temp, genExpr(base), genExpr(index), "", expr->lineno);
		return temp;
	}

	if (expr->type == "LengthFunction") {
		if (expr->children.empty()) {
			return "";
		}
		string temp = newTemp();
		emit(IROp::Length, temp, genExpr(expr->children.front()), "", "", expr->lineno);
		return temp;
	}

	if (expr->type == "ListExpression") {
		auto it = expr->children.begin();
		Node* typeNode = (it != expr->children.end()) ? *it : nullptr;
		vector<string> values;
		if (it != expr->children.end()) {
			++it;
		}
		for (; it != expr->children.end(); ++it) {
			if (*it) {
				values.push_back(genExpr(*it));
			}
		}

		string serialized;
		for (size_t index = 0; index < values.size(); ++index) {
			if (index > 0) {
				serialized += ", ";
			}
			serialized += values[index];
		}

		string elementType = typeNode ? typeNode->value : "unknown";
		string temp = newTemp();
		emit(IROp::NewArray,
			 temp,
			 elementType,
			 to_string(values.size()),
			 serialized,
			 expr->lineno);
		return temp;
	}

	if (expr->type == "FunctionCall") {
		bool isMethodCall = !expr->children.empty() && expr->children.front() && expr->children.front()->type != "Expression";
		string receiver;
		Node* argsNode = nullptr;

		if (isMethodCall) {
			receiver = genExpr(expr->children.front());
			auto it = expr->children.begin();
			++it;
			argsNode = (it != expr->children.end()) ? *it : nullptr;
		} else if (!expr->children.empty()) {
			argsNode = expr->children.front();
		}

		int argCount = 0;
		if (argsNode && argsNode->type == "Expression") {
			for (Node* arg : argsNode->children) {
				if (!arg) {
					continue;
				}
				emit(IROp::Param, "", genExpr(arg), "", "", expr->lineno);
				++argCount;
			}
		}

		string temp = newTemp();
		if (!isMethodCall && classNames.find(expr->value) != classNames.end()) {
			emit(IROp::NewObject, temp, "", "", expr->value, expr->lineno);
			return temp;
		}

		string callee = isMethodCall ? expr->value : toFunctionLabel(currentClassName, expr->value);
		emit(IROp::Call, temp, receiver, to_string(argCount), callee, expr->lineno);
		return temp;
	}

	if (expr->type == "Expression" && !expr->children.empty()) {
		return genExpr(expr->children.front());
	}

	return expr->value;
}
