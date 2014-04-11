#ifndef TS_SCRIPT_H_
#define TS_SCRIPT_H_

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include "ts_script.tab.hpp"

extern int yylineno;

class ASTNode {
public:
	int type_;
	ASTNode *left_;
	ASTNode *right_;
	const char *printable_node_type_;
public:
	ASTNode(int type, ASTNode *left, ASTNode *right,
			const char *printable_node_type) :
			type_(type), left_(left), right_(right), printable_node_type_(
					printable_node_type) {
		fprintf(stdout, "Creating AST with type %s (%d)\n",
				printable_node_type_, type);
	}

	virtual void PrintDotNull(std::string label, int nullcount,
			std::ofstream& dotfile) {
		std::ostringstream ostr;
		ostr << nullcount;
		dotfile << "    " << label;
	}

	virtual void PrintDotAux(std::ofstream& dotfile) {
		static int nullcount = 0;

		if (left()) {
			dotfile << "    " << str() << " -> " << left()->str() << ";\n";
			left()->PrintDotAux(dotfile);
		} else
			PrintDotNull(str(), nullcount++, dotfile);

		if (right()) {
			dotfile << "    " << str() << " -> " << right()->str() << ";\n";
			right()->PrintDotAux(dotfile);
		} else
			PrintDotNull(str(), nullcount++, dotfile);
	}

	virtual void printDot(std::ofstream& dotfile) {
		dotfile << "digraph BST {\n";
		dotfile << "    node [fontname=\"Arial\"];\n";

		if (!right() && !left())
			dotfile << "    " << str() << ";\n";
		else
			PrintDotAux(dotfile);

		dotfile << "}\n";
	}

	virtual std::string str() {
		std::stringstream ss;
		ss << "\"" << printable_node_type_ << " " << this << "\"";
		return ss.str();
	}

	ASTNode *left() {
		return left_;
	}

	ASTNode *right() {
		return right_;
	}

	int type() {
		return type_;
	}

	virtual ~ASTNode() {
	}
};

class StringLiteralNode: public ASTNode {
public:
	std::string value_;
public:
	StringLiteralNode(std::string value) :
			value_(value), ASTNode(STRING_LITERAL, NULL, NULL, "STRING_LITERAL") {
		fprintf(stdout, "Creating String Literal: %s\n", value_.c_str());
	}

	std::string value() {
		return value_;
	}

	virtual std::string str() {
		std::string s = value_.substr(1, value_.length() - 2);
		return std::string("\"STRING=" + s + "\"");
	}
};

class IdentifierNode: public ASTNode {
public:
	std::string value_;
public:
	IdentifierNode(std::string value) :
			value_(value), ASTNode(IDENTIFIER, NULL, NULL, "IDENTIFIER") {
		fprintf(stdout, "Creating Identifier: %s\n", value_.c_str());
	}
	std::string value() {
		return value_;
	}
	virtual std::string str() {
		return std::string("\"IDENTIFIER=" + value() + "\"");
	}
};

class RegexNode: public ASTNode {
public:
	std::string full_regex_;
	std::string regex_str_;
	std::vector<char> modifiers;
public:
	RegexNode(std::string full_regex) :
			full_regex_(full_regex), ASTNode(REGEX, NULL, NULL, "REGEX") {
		fprintf(stdout, "Creating Regex: %s\n", full_regex_.c_str());
		std::string::size_type last_slash = 0;
		for (last_slash = full_regex_.size();
				full_regex_[last_slash] != '/'; --last_slash) {
			fprintf(stdout, "Adding modifier: %c\n",
					full_regex_[last_slash - 1]);
			modifiers.push_back(full_regex_[last_slash]);
		}

		for (std::string::size_type i = 0; i != full_regex_.size(); ++i) {
			if (full_regex_[i] == '/') {
				regex_str_ = full_regex_.substr(i + 1, last_slash - i - 2);
				fprintf(stdout, "regex_str=%s\n", regex_str_.c_str());
				break;
			}
		}

	}

	std::string regex_value() {
		return regex_str_;
	}

	std::string value() {
		return full_regex_;
	}
	virtual std::string str() {
		return std::string("\"REGEX=" + value() + "\"");
	}
};

#endif /* TS_SCRIPT_H_ */
