
%{
  #include <cstdio>
  #include "ts_script.h"
  extern "C" int yyparse(ASTNode **);
  extern "C" int yylex(void);
  extern "C" int yywrap(void) { return 1; }
  extern "C" void yyerror(ASTNode **, const char *, ...);
  #define New_AstNode(T, L, R) new ASTNode(T, L, R, #T)
  #define New_StringLiteral(S) new StringLiteralNode(S)
  #define New_Identifier(I) new IdentifierNode(I)
  #define New_RegexNode(R) new RegexNode(R)
%}

%union 
{
        int v_int;
        char *v_string;
        float v_float;
        struct ASTNode *v_ASTNode;
}

%token  PLUGIN OPEN_BRACE CLOSE_BRACE OPEN_BRACKET CLOSE_BRACKET
%token  <v_string> STRING_LITERAL IDENTIFIER VAR REGEX
%token  READ_REQUEST_HEADERS SEND_REQUEST_HEADERS
%token  READ_RESPONSE_HEADERS SEND_RESPONSE_HEADERS
%token  TERM HOOK HEADER OPEN_PAREN CLOSE_PAREN
%token  CLIENT_REQUEST CLIENT_RESPONSE
%token  SERVER_REQUEST SERVER_RESPONSE
%token  EQUAL_EQUAL EQUAL_TILDE NOT_EQUAL URL SCHEME PATH PORT DOMAIN QUERY
%token  EQUAL PLUS_EQUAL NOTHING IF ELSE

// these tokens are added because we want to use them in the AST
%token  OBJECT HEADER_PROPERTY BODY HOOKS BODIES IF_ELSE URL_PROPERTY

%type <v_ASTNode> hooks bodies plugin hook_type hook body conditional if_block else_block
%type <v_ASTNode> statement assignment lvalue rvalue request_response_object comparison
%type <v_ASTNode> header_property object request_response_property plugin_declaration
%type <v_ASTNode> url_key url_property declaration identifier regex

%parse-param {ASTNode **root}
%start plugin


%%

plugin
	: plugin_declaration hooks { *root = New_AstNode(PLUGIN, $1, $2); }
	;

plugin_declaration
 	: PLUGIN STRING_LITERAL TERM { 
 			$$ = New_StringLiteral($2);
 			free($2); 
 		}
 	;
 	
hooks
 	: { $$ = New_AstNode(NULL, NULL, NULL); }
 	| hooks hook { $$ = New_AstNode(HOOKS, $1, $2); }
	;
	
hook
 	: HOOK hook_type OPEN_BRACE bodies CLOSE_BRACE { $$ = New_AstNode(HOOK, $2, $4); }
 	;
 	
bodies
 	: { $$ = New_AstNode(NULL, NULL, NULL); }
 	| bodies body { $$ = New_AstNode(BODIES, $1, $2); }
 	;

body
 	: statement
 	| assignment
 	| conditional
 	| hook
 	;
 	
object
	: request_response_object request_response_property { $$ = New_AstNode(OBJECT, $1, $2); }

assignment
	: lvalue EQUAL rvalue TERM { $$ = New_AstNode(EQUAL, $1, $3); }
	| lvalue PLUS_EQUAL rvalue TERM { $$ = New_AstNode(PLUS_EQUAL, $1, $3); }
	;

conditional
	: if_block else_block { $$ = New_AstNode(IF_ELSE, $1, $2); }
	| if_block { $$ = New_AstNode(IF, $1, NULL); }
	;
	
if_block
	: IF OPEN_PAREN comparison CLOSE_PAREN OPEN_BRACE bodies CLOSE_BRACE { $$ = New_AstNode(IF, $3, $6); }
	;
	
else_block
	: ELSE OPEN_BRACE bodies CLOSE_BRACE { $$ = New_AstNode(ELSE, $3, NULL); }
	;	
	
comparison
	: rvalue EQUAL_EQUAL rvalue { $$ = New_AstNode(EQUAL_EQUAL, $1, $3); }
	| rvalue NOT_EQUAL rvalue  { $$ = New_AstNode(NOT_EQUAL, $1, $3); }
	| rvalue EQUAL_TILDE regex { $$ = New_AstNode(EQUAL_TILDE, $1, $3); }
	;
	
statement
	: { $$ = New_AstNode(NULL, NULL, NULL); }
	;
	
lvalue
	: object
	| declaration
	| identifier
	;

regex
	: REGEX { 
			  $$ = New_RegexNode($1);
			  free($1);
		    }
	;
	
declaration
	: VAR identifier { $$ = New_AstNode(VAR, $2, NULL); }
	;

identifier
	: IDENTIFIER { 
				$$ = New_Identifier($1); 
				free($1);
			}
	;
		
rvalue
	: object
	| STRING_LITERAL { 
			$$ = New_StringLiteral($1); 
			free($1);
		}
	| identifier
	| NOTHING { $$ = New_AstNode(NULL, NULL, NULL); }
	;	

request_response_property
	: url_property 
	| header_property
	;
	
header_property
	: HEADER OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET { 
			$$ = New_AstNode(HEADER, New_StringLiteral($3), NULL); 
			free($3);
		}
	;

url_property
	: URL OPEN_BRACKET url_key CLOSE_BRACKET { $$ = New_AstNode(URL, $3, NULL); }
	;

url_key
	: SCHEME { $$ = New_AstNode(SCHEME, NULL, NULL); }
	| DOMAIN { $$ = New_AstNode(DOMAIN, NULL, NULL); }
	| PORT { $$ = New_AstNode(PORT, NULL, NULL); }
	| PATH { $$ = New_AstNode(PATH, NULL, NULL); }
	| QUERY { $$ = New_AstNode(QUERY, NULL, NULL); }
	;

request_response_object
	: CLIENT_REQUEST { $$ = New_AstNode(CLIENT_REQUEST, NULL, NULL); }
	| CLIENT_RESPONSE { $$ = New_AstNode(CLIENT_RESPONSE, NULL, NULL); }
	| SERVER_REQUEST { $$ = New_AstNode(SERVER_REQUEST, NULL, NULL); }
	| SERVER_RESPONSE { $$ = New_AstNode(SERVER_RESPONSE, NULL, NULL); }
	;
	
hook_type
 	: READ_REQUEST_HEADERS { $$ = New_AstNode(READ_REQUEST_HEADERS, NULL, NULL); }
 	| SEND_REQUEST_HEADERS { $$ = New_AstNode(SEND_REQUEST_HEADERS, NULL, NULL); }
	| READ_RESPONSE_HEADERS { $$ = New_AstNode(READ_RESPONSE_HEADERS, NULL, NULL); }
	| SEND_RESPONSE_HEADERS { $$ = New_AstNode(SEND_RESPONSE_HEADERS, NULL, NULL); }
 	;
	
%%