#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <fstream>
#include "ts_script.h"
#include "ts_script_codegen.h"

extern "C" int yyparse(ASTNode **);

extern "C" void yyerror(ASTNode **node, const char *s, ...) {
	va_list ap;
	va_start(ap, s);

	fprintf(stderr, "line %d: ", yylineno);
	vfprintf(stderr, s, ap);
	fprintf(stderr, "\n");
}

int main(int argc, char **argv) {
	ASTNode *root = NULL;
	int ret_val = yyparse(&root);
	fprintf(stdout, "-----------------\n");
	std::ofstream outputFile("parse_tree.dot");
	root->printDot(outputFile);
	outputFile.close();

	std::ofstream outputCode("plugin.c");
	CCompiledTarget target(outputCode);
	target.generate(root);
	outputCode.close();

	return 0;
}
