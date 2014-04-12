#include "ts_script.h"
#include "ts_script_codegen.h"
#include <cassert>
#include <iostream>
using std::endl;

#define ASSERT_NODE_NON_NULL(n) \
	assert(n);

#define ASSERT_NODE_TYPE(n, t) \
	ASSERT_NODE_NON_NULL(n) \
	if (n->type() != t) { assert(! #n "!=" #t); }

CCompiledTarget::CCompiledTarget(std::ofstream &outfile) :
    outfile_(outfile), plugin_init_code_(new std::ostringstream()), global_plugin_registrations_(
        new std::ostringstream()), globals_(new std::ostringstream()), hook_methods_(
        new std::ostringstream()), in_global_hook_(false), hook_counter_(0), global_count_(
        0), pcre_count_(0), has_pcre_(false) {
  indentation_chunk_ = "  ";
}

void CCompiledTarget::generate(ASTNode *node) {
  ASSERT_NODE_TYPE(node, PLUGIN);
  generate_for_node(node);
  write_header_section();
}

void CCompiledTarget::generate_for_node(ASTNode *node) {
  if (node == NULL) {
    fprintf(stdout, "Encountered NULL node\n");
  }
  if (node->type() == PLUGIN) {
    ASSERT_NODE_TYPE(node->left(), STRING_LITERAL);
    std::string quoted_name =
        static_cast<StringLiteralNode*>(node->left())->value();
    if (quoted_name.length() > 2) {
      plugin_id_ = quoted_name.substr(1, quoted_name.length() - 2);
    }
    fprintf(stdout, "Plugin name = %s\n", plugin_id_.c_str());
    generate_for_node(node->right());
  } else if (node->type() == HOOKS) {
    generate_for_node(node->left());
    generate_for_node(node->right());
  } else if (node->type() == HOOK) {
    ASSERT_NODE_NON_NULL(node->left());
    if (node->left()->type() == READ_REQUEST_HEADERS
        || node->left()->type() == SEND_REQUEST_HEADERS
        || node->left()->type() == READ_RESPONSE_HEADERS
        || node->left()->type() == SEND_RESPONSE_HEADERS) {

      if (node->right()->type() == 0) {
        return;	// Empty hook, just NOP it.
      }
      bool global = true;
      if (hook_code_.empty()) {
        global_hooks_.insert(node->left()->type());
      } else {
        global = false;
      }

      // Any nested hooks automatically become transaction hooks
      fprintf(stdout, "\tEntering %s hook %s\n",
          global ? "global" : "transaction",
          node->left()->printable_node_type_);
      generate_for_hook_node(node);
      fprintf(stdout, "\tExiting %s hook %s\n",
          global ? "global" : "transaction",
          node->left()->printable_node_type_);
      in_global_hook_ = false;
    } else {
      fprintf(stderr, "Invalid hook type %s (%d)\n",
          node->left()->printable_node_type_, node->left()->type());
    }
  } else if (node->type() == BODIES) {
    generate_for_node(node->left());
    generate_for_node(node->right());
  } else if (node->type() == EQUAL || node->type() == PLUS_EQUAL) {
    generate_for_assignment_node(node);
  } else if (node->type() == IF) {
    generate_for_if(node->left());
  } else if (node->type() == IF_ELSE) {
    generate_for_if(node->left(), false);
    generate_for_else(node->right());
  } else if (node->type() == 0) {
    return;
  }
}

void CCompiledTarget::generate_for_if(ASTNode *node, bool trailing_nl) {
  ASSERT_NODE_TYPE(node, IF);
  generate_for_comparison(node, trailing_nl);
}

void CCompiledTarget::generate_for_else(ASTNode *node) {
  // this is fairly easy.
  ASSERT_NODE_TYPE(node, ELSE);
  std::ostringstream *stream = hook_code_.top();
  *stream << " else { " << endl;
  left_indentation_ += indentation_chunk_;
  generate_for_node(node->left());
  left_indentation_ = left_indentation_.substr(0,
      left_indentation_.length() - indentation_chunk_.size());
  *stream << left_indentation_ << "}" << endl;
}

void CCompiledTarget::generate_for_comparison(ASTNode *node, bool trailing_nl) {
  std::ostringstream *stream = hook_code_.top();
  std::string left_rvalue = build_rvalue(node->left()->left());

  std::string right_rvalue;
  if (node->left()->right()->type() != REGEX)
    right_rvalue = build_rvalue(node->left()->right());

  // Now generate the if body
  if (node->left()->type() == EQUAL_EQUAL) {
    *stream << left_indentation_ << "if (strcmp(" << left_rvalue << ", "
        << right_rvalue << ") == 0) {" << endl;
  } else if (node->left()->type() == NOT_EQUAL) {
    *stream << left_indentation_ << "if (strcmp(" << left_rvalue << ", "
        << right_rvalue << ") != 0) {" << endl;
  } else if (node->left()->type() == EQUAL_TILDE) {
    ASSERT_NODE_TYPE(node->left()->right(), REGEX);
    generate_for_pcre(node->left()->right(), node->left()->left());
  }
  // Generate the body
  left_indentation_ += "  ";
  generate_for_node(node->right());
  left_indentation_ = left_indentation_.substr(0,
      left_indentation_.length() - 2);
  *stream << left_indentation_ << "}";
  if (trailing_nl)
    *stream << endl;

}

void CCompiledTarget::generate_for_pcre(ASTNode *node, ASTNode *cmp_node) {
  ASSERT_NODE_TYPE(node, REGEX);
  has_pcre_ = true;

  RegexNode *regex_node = static_cast<RegexNode*>(node);
  std::ostringstream regex_var_name;
  std::ostringstream regex_ret_var_name;
  std::ostringstream regex_extra_var_name;
  std::ostringstream regex_ret_vector_name;
  std::ostringstream *stream = NULL;
  regex_var_name << "REGEX_" << pcre_count_;
  regex_extra_var_name << "REGEX_EXTRA_" << pcre_count_;
  regex_ret_var_name << "pcre_exec_ret_" << pcre_count_;
  regex_ret_vector_name << "pcre_exec_ret_vec_" << pcre_count_;

  // Create the PCRE variables
  *globals_ << "pcre *" << regex_var_name.str() << " = NULL;" << endl;
  *globals_ << "pcre_extra *" << regex_extra_var_name.str() << " = NULL;"
      << endl;
  std::string pcre_str_global = create_global(
      std::string("\"") + regex_node->regex_value() + std::string("\""));
  std::string cmp_var;
  if (cmp_node->type() == STRING_LITERAL) {
    cmp_var = create_global(static_cast<StringLiteralNode*>(cmp_node)->value());
  } else if (cmp_node->type() == IDENTIFIER) {
    cmp_var = build_var_name(cmp_node);
  } else if (cmp_node->type() == OBJECT) {
    std::ostringstream local_var;
    local_var << "var_pcre_input_" << pcre_count_;
    stream = hook_code_.top();
    *stream << left_indentation_ << "const char *" << local_var.str() << " = "
        << build_rvalue(cmp_node) << ";" << endl;
    cmp_var = local_var.str();
  }
  pcre_count_++;

  // Add the PCRE compilation code to TSPluginInit()
  stream = plugin_init_code_;
  *stream << indentation_chunk_ << "pcre_error_str = NULL;" << endl;
  *stream << indentation_chunk_ << "pcre_error_offset = 0;" << endl;
  *stream << indentation_chunk_ << regex_var_name.str() << " = pcre_compile("
      << pcre_str_global << ", 0, &pcre_error_str, &pcre_error_offset, NULL);"
      << endl;
  *stream << indentation_chunk_ << "if (" << regex_var_name.str()
      << " == NULL) {" << endl;
  *stream << indentation_chunk_ << indentation_chunk_
      << "TSError(\"ERROR: Could not compile regex '%s': %s\\n\", "
      << pcre_str_global << ", pcre_error_str);" << endl;
  *stream << indentation_chunk_ << indentation_chunk_ << "exit(1);" << endl
      << indentation_chunk_ << "}" << endl;

  // Next, add the study code
  *stream << indentation_chunk_ << regex_extra_var_name.str()
      << " = pcre_study(" << regex_var_name.str() << ", 0, &pcre_error_str);"
      << endl;
  *stream << indentation_chunk_ << "if (pcre_error_str != NULL) {" << endl;
  *stream << indentation_chunk_ << indentation_chunk_
      << "TSError(\"ERROR: Could not study regex '%s': %s\\n\", "
      << pcre_str_global << ", pcre_error_str);" << endl;
  *stream << indentation_chunk_ << indentation_chunk_ << "exit(1);" << endl
      << indentation_chunk_ << "}" << endl;

  // Build the options field
  std::ostringstream regex_options;
  regex_options << "0";

  for (int i = 0; i < regex_node->modifiers.size(); ++i) {
    switch (regex_node->modifiers[i]) {
    case 'i': {
      regex_options << " | PCRE_CASELESS";
      break;
    }
    case 's': {
      regex_options << " | PCRE_DOTALL";
      break;
    }
    case 'x': {
      regex_options << " | PCRE_EXTENDED";
      break;
    }
    case 'm': {
      regex_options << " | PCRE_MULTILINE";
      break;
    }
    default:
      fprintf(stderr, "Unknown regex modifier: %c\n", regex_node->modifiers[i]);
    }
  }
  // Now insert the regex code.
  stream = hook_code_.top();
  *stream << left_indentation_ << "int " << regex_ret_var_name.str() << " = 0;"
      << endl;
  *stream << left_indentation_ << "int " << regex_ret_vector_name.str()
      << "[REGEX_VECTOR_SIZE];" << endl;
  *stream << left_indentation_ << regex_ret_var_name.str() << " = pcre_exec("
      << regex_var_name.str() << ", " << regex_extra_var_name.str() << ", "
      << cmp_var << ", strlen(" << cmp_var << "), 0, " << regex_options.str()
      << ", " << regex_ret_vector_name.str() << ", REGEX_VECTOR_SIZE);" << endl;
  *stream << left_indentation_ << "if (" << regex_ret_var_name.str()
      << " >= 0) {" << endl;

}
void CCompiledTarget::generate_for_hook_node(ASTNode *node) {
  std::ostringstream method_name;
  std::ostringstream *stream = NULL;
  method_name << "hook_" << std::string(node->left()->printable_node_type_)
      << "_" << hook_counter_++;
  bool global = hook_code_.empty();

  if (global) {
    stream = global_plugin_registrations_;
    // Dont use left indentation since we know we're in TSPluginInit
    *stream << indentation_chunk_ << "TSHttpHookAdd(";
  } else {
    stream = hook_code_.top();
    *stream << left_indentation_ << "TSHttpTxnHookAdd(txnp, ";
  }
  *stream << "TS_HTTP_" << std::string(node->left()->printable_node_type_);
  *stream << ", TSContCreate(" << method_name.str() << ", TSMutexCreate()));"
      << endl;

  hook_code_.push(new std::ostringstream()); // this is where we'll accumulate the hook code.
  stream = hook_code_.top();
  *stream << "static int " << method_name.str()
      << "(TSCont contp, TSEvent event, void *edata) {" << endl;

  std::string cur_indentation = left_indentation_;
  left_indentation_ = indentation_chunk_;

  *stream << left_indentation_ << "TSHttpTxn txnp = (TSHttpTxn) edata;" << endl;
  generate_for_node(node->right());

  *stream << left_indentation_
      << "TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);" << endl;

  left_indentation_ = cur_indentation;

  hook_code_.pop();
  *stream << "}" << endl;
  // At this point we can commit to the hook.
  //fprintf(stdout, "Completed code generation:\n%s\n", stream->str().c_str());
  *hook_methods_ << stream->str() << endl;
  delete stream;
}

std::string CCompiledTarget::build_rvalue(ASTNode *node) {
  std::ostringstream rval;
  fprintf(stdout, "rvalue type: %s\n", node->printable_node_type_);

  if (node->type() == STRING_LITERAL) {
    std::string var_name = create_global(
        static_cast<StringLiteralNode*>(node)->value());
    rval << var_name;
  } else if (node->type() == OBJECT) {
    if (node->right()->type() == HEADER) {
      ASSERT_NODE_TYPE(node->right()->left(), STRING_LITERAL);
      std::string header_name = create_global(
          static_cast<StringLiteralNode*>(node->right()->left())->value());
      std::string object_type = request_response_object_type(node->left());
      rval << "get_header(txnp, " << node->left()->printable_node_type_ << ", "
          << header_name << ")";
    } else if (node->right()->type() == URL) {
      int url_prop = node->right()->left()->type();
      if (url_prop != PATH && url_prop != PORT && url_prop != SCHEME
          && url_prop != DOMAIN && url_prop != QUERY) {
        assert(!"Invalid url property");
      }
      std::string object_type = request_response_object_type(node->left());
      rval << "get_url_property(txnp, " << object_type << ", "
          << node->right()->left()->printable_node_type_ << ")";
    } else {
      assert(!"unknown object type");
    }
  } else if (node->type() == IDENTIFIER) {
    rval << build_var_name(node);
  } else {
    assert(!"unknown rvalue");
  }
  return rval.str();
}
void CCompiledTarget::generate_for_assignment_node(ASTNode *node) {
  // We're dealing with an assignment, let's figure out what is being assigned to what
  std::ostringstream *stream = hook_code_.top();

  // We will consider the lvalue and the rvalue.
  // If the lvalue is an object (such as headers) then we're just setting
  // a header value to the rvalue.
  // Let's start with figure out what's going on with the rvalue.
  // It can be a string literal or an object.

  std::ostringstream rvalue_assignment;
  rvalue_assignment << build_rvalue(node->right());

  // What are we assigning to?
  if (node->left()->type() == OBJECT) {
    generate_lvalue_assignment(node->left(), node, &rvalue_assignment);
  } else if (node->left()->type() == IDENTIFIER) {
    *stream << left_indentation_ << build_var_name(node->left()) << " = "
        << rvalue_assignment.str() << endl;
  } else if (node->left()->type() == VAR) {
    ASSERT_NODE_TYPE(node->left()->left(), IDENTIFIER);
    *stream << left_indentation_ << "char *"
        << build_var_name(node->left()->left()) << " = "
        << rvalue_assignment.str() << ";" << endl;
  } else {
    assert(!"unknown lvalue!");
  }

}

std::string CCompiledTarget::build_var_name(ASTNode *node) {
  ASSERT_NODE_TYPE(node, IDENTIFIER);
  IdentifierNode *ident_node = static_cast<IdentifierNode*>(node);
  std::ostringstream stream;
  stream << "var_" << ident_node->value();
  return stream.str();
}

std::string CCompiledTarget::create_global(std::string val) {
  std::ostringstream var_name;
  // No need to create a new global if one exists with this value.
  if (existing_globals_.count(val)) {
    return existing_globals_[val];
  } else {
    var_name << "STR_" << global_count_++;
    *globals_ << "const char *" << var_name.str() << " = " << val << ";"
        << endl;
    existing_globals_[val] = var_name.str();
    return var_name.str();
  }
}

void CCompiledTarget::generate_lvalue_assignment(ASTNode *node,
    ASTNode *assignment_node, std::ostringstream *assign_val) {
  ASSERT_NODE_TYPE(node, OBJECT);
  std::string action;
  std::ostringstream *stream = hook_code_.top();

  if (node->right()->type() == HEADER) {
    std::string header_name;

    if (node->right()->left()->type() == STRING_LITERAL) {
      header_name = create_global(
          static_cast<StringLiteralNode*>(node->right()->left())->value());
    } else if (node->right()->left()->type() == IDENTIFIER) {
        header_name = build_var_name(node->right()->left());
    }

    if (assignment_node->type() == EQUAL) {
      action = "set_header";
    } else if (assignment_node->type() == PLUS_EQUAL) {
      action = "append_header";
    }
    *stream << left_indentation_ << action << "(txnp, "
        << node->left()->printable_node_type_ << ", " << header_name << ", "
        << assign_val->str() << ");" << endl;
  } else if (node->right()->type() == URL) {
    if (assignment_node->type() == EQUAL) {
      action = "set_url_propety";
    } else if (assignment_node->type() == PLUS_EQUAL) {
      action = "append_url_property";
    }
    *stream << left_indentation_ << action << "(txnp, "
        << node->left()->printable_node_type_ << ", "
        << node->right()->left()->printable_node_type_ << ", "
        << assign_val->str() << ");" << endl;

  } else {
    assert(!"Unknown object type");
  }
}

std::string CCompiledTarget::request_response_object_type(ASTNode *node) {
  std::string object_type;
  switch (node->type()) {
  case SERVER_REQUEST:
    object_type = "SERVER_REQUEST";
    break;
  case SERVER_RESPONSE:
    object_type = "SERVER_RESPONSE";
    break;
  case CLIENT_REQUEST:
    object_type = "CLIENT_REQUEST";
    break;
  case CLIENT_RESPONSE:
    object_type = "CLIENT_RESPONSE";
    break;
  default:
    assert(!"unknown object type");
  }
  return object_type;
}

void CCompiledTarget::write_header_section() {
  outfile_ << "#include <ts/ts.h>" << endl;
  outfile_ << "#include <string.h>" << endl;
  outfile_ << "#include <stdio.h>" << endl;
  if (has_pcre_) {
    outfile_ << "#include <pcre.h>" << endl;
    outfile_ << "#define REGEX_VECTOR_SIZE 30" << endl;
  }
  outfile_ << "const char *PLUGIN_ID = \"" << plugin_id_ << "\";" << endl;
  //outfile_ << "typedef enum { SERVER_REQUEST = 0, SERVER_RESPONSE, CLIENT_REQUEST, CLIENT_RESPONSE} RequestResponseType;" << endl;
  outfile_ << globals_->str();
  outfile_ << endl;
  outfile_ << hook_methods_->str();
  outfile_ << endl;
  outfile_ << "void TSPluginInit(int argc, const char *argv[]) {" << endl;
  write_registration_code();
  outfile_ << plugin_init_code_->str();
  outfile_ << global_plugin_registrations_->str();
  outfile_ << "}" << endl;
}

void CCompiledTarget::write_registration_code() {
  outfile_ << indentation_chunk_ << "TSPluginRegistrationInfo info;" << endl;
  outfile_ << indentation_chunk_ << "info.plugin_name = PLUGIN_ID;" << endl;
  outfile_ << indentation_chunk_ << "info.vendor_name = PLUGIN_ID;" << endl;
  outfile_ << indentation_chunk_ << "info.support_email = PLUGIN_ID;" << endl;
  outfile_ << indentation_chunk_
      << "if (TSPluginRegister(TS_SDK_VERSION_3_0, &info) != TS_SUCCESS) {"
      << endl;
  outfile_ << indentation_chunk_ << "  "
      << "TSError(\"Plugin registration failed.\\n\");" << endl;
  outfile_ << indentation_chunk_ << "}" << endl;
}

CCompiledTarget::~CCompiledTarget() {

}
