#ifndef TS_SCRIPT_CODEGEN_H_
#define TS_SCRIPT_CODEGEN_H_

#include <set>
#include <fstream>
#include <sstream>
#include <stack>
#include <map>
#include "ts_script.h"

class CompiledTarget {
public:
	virtual void generate(ASTNode *root) = 0;
	virtual ~CompiledTarget() { };
};

/*
 * Walk the AST generating C code
 */
class CCompiledTarget: public CompiledTarget {
protected:
	std::ofstream &outfile_;
	std::string plugin_id_;
	std::set<int> global_hooks_;
	std::map<std::string, std::string> existing_globals_;
	std::stack<std::ostringstream *> hook_code_;
	std::ostringstream *globals_;
	std::ostringstream *plugin_init_code_;
	std::ostringstream *global_plugin_registrations_;
	std::ostringstream *hook_methods_;
	std::string left_indentation_;
	std::string indentation_chunk_;
	bool in_global_hook_;
	int hook_counter_;
	int global_count_;
	int pcre_count_;
	bool has_pcre_;
public:
	CCompiledTarget(std::ofstream& outfile);
	virtual void generate(ASTNode *);
	virtual ~CCompiledTarget();
private:
	std::string create_global(std::string val);
	std::string request_response_object_type(ASTNode *);
	std::string build_rvalue(ASTNode *);
	std::string build_var_name(ASTNode *);
	void generate_for_node(ASTNode *);
	void generate_for_pcre(ASTNode *, ASTNode *);
	void generate_for_hook_node(ASTNode *);
	void generate_for_assignment_node(ASTNode *);
	void generate_lvalue_assignment(ASTNode *, ASTNode *, std::ostringstream *assignment);
	void generate_for_if(ASTNode *, bool trailing_nl = true);
	void generate_for_else(ASTNode *);
	void generate_for_comparison(ASTNode *, bool trailing_nl = true);
	void write_header_section();
	void write_registration_code();
};


#endif
