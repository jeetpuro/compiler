#ifndef CFGDOTWRITER_H
#define CFGDOTWRITER_H

#include "IR.h"
#include <fstream>
#include <sstream>
#include <string>

namespace CFGDotWriter {

inline std::string escapeDot(const std::string& text) {
	std::string escaped;
	for (char ch : text) {
		if (ch == '"') {
			escaped += "\\\"";
		} else if (ch == '\n') {
			escaped += "\\n";
		} else {
			escaped += ch;
		}
	}
	return escaped;
}

inline std::string tacToString(const TAC& tac) {
	switch (tac.op) {
	case IROp::Assign:
		return tac.dst + " := " + tac.src1;
	case IROp::Add:
		return tac.dst + " := " + tac.src1 + " + " + tac.src2;
	case IROp::Sub:
		return tac.dst + " := " + tac.src1 + " - " + tac.src2;
	case IROp::Mul:
		return tac.dst + " := " + tac.src1 + " * " + tac.src2;
	case IROp::Div:
		return tac.dst + " := " + tac.src1 + " / " + tac.src2;
	case IROp::Pow:
		return tac.dst + " := " + tac.src1 + " ^ " + tac.src2;
	case IROp::CmpLT:
		return tac.dst + " := " + tac.src1 + " < " + tac.src2;
	case IROp::CmpLE:
		return tac.dst + " := " + tac.src1 + " <= " + tac.src2;
	case IROp::CmpGT:
		return tac.dst + " := " + tac.src1 + " > " + tac.src2;
	case IROp::CmpGE:
		return tac.dst + " := " + tac.src1 + " >= " + tac.src2;
	case IROp::CmpEQ:
		return tac.dst + " := " + tac.src1 + " == " + tac.src2;
	case IROp::CmpNE:
		return tac.dst + " := " + tac.src1 + " != " + tac.src2;
	case IROp::And:
		return tac.dst + " := " + tac.src1 + " && " + tac.src2;
	case IROp::Or:
		return tac.dst + " := " + tac.src1 + " || " + tac.src2;
	case IROp::Not:
		return tac.dst + " := !" + tac.src1;
	case IROp::Goto:
		return "goto B" + tac.extra;
	case IROp::IfFalseGoto:
		return "iffalse " + tac.src1 + " goto B" + tac.extra;
	case IROp::Param:
		return "param " + tac.src1;
	case IROp::Call:
		if (tac.dst.empty()) {
			return "call " + tac.extra + ", " + tac.src2;
		}
		if (tac.src1.empty()) {
			return tac.dst + " := call " + tac.extra + ", " + tac.src2;
		}
		return tac.dst + " := call " + tac.src1 + "." + tac.extra + ", " + tac.src2;
	case IROp::Return:
		return tac.src1.empty() ? "return" : "return " + tac.src1;
	case IROp::Print:
		return "print " + tac.src1;
	case IROp::Read:
		return "read " + tac.dst;
	case IROp::ArrayLoad:
		return tac.dst + " := " + tac.src1 + "[" + tac.src2 + "]";
	case IROp::ArrayStore:
		return tac.dst + "[" + tac.src1 + "] := " + tac.src2;
	case IROp::Length:
		return tac.dst + " := length " + tac.src1;
	case IROp::NewObject:
		return tac.dst + " := new " + tac.extra;
	case IROp::NewArray:
		if (tac.extra.empty()) {
			return tac.dst + " := new " + tac.src1 + ", " + tac.src2;
		}
		return tac.dst + " := new " + tac.src1 + ", " + tac.src2 + " {" + tac.extra + "}";
	}
	return "<unknown tac>";
}

inline void write(const ProgramIR& ir, const std::string& filename) {
	std::ofstream out(filename);
	out << "digraph CFG {\n";
	out << "  node [shape=box, fontname=\"Courier\"];\n";

	for (const auto& kv : ir.functions) {
		const FunctionIR& function = kv.second;
		out << "  subgraph cluster_" << escapeDot(function.name) << " {\n";
		out << "    label=\"" << escapeDot(function.name) << "\";\n";

		for (const BasicBlock& block : function.blocks) {
			std::ostringstream label;
			label << block.name << "\\n";
			for (const TAC& tac : block.code) {
				label << tacToString(tac) << "\\l";
			}
			out << "    B" << block.id << " [label=\"" << escapeDot(label.str()) << "\"];\n";
		}

		for (const BasicBlock& block : function.blocks) {
			for (int succ : block.succ) {
				out << "    B" << block.id << " -> B" << succ << ";\n";
			}
		}

		out << "  }\n";
	}

	out << "}\n";
}

} // namespace CFGDotWriter

#endif
