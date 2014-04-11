%{
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include "ts_script.tab.hpp"
extern YYSTYPE yylval;
extern "C" int yylex();
extern "C" void yyerror(ASTNode **, const char *, ...); 
extern "C" int yywrap(void);
static void comment(void);

#define DBG_PRINT(x) \
 do { fprintf(stdout, "[DEBUG] Consumed (%d bytes): %s (%s)\n", (int)strlen(yytext), yytext, #x); } while (0);
%}

WS  [ \t\v\n\f]
LETTER [a-zA-Z]
DIGIT [0-9]
UNDERSCORE _

%%

"/*"  { comment(); DBG_PRINT(COMMENT);  }

"plugin" { DBG_PRINT(PLUGIN); return PLUGIN; }
"hook" { DBG_PRINT(HOOK); return HOOK; }
"read_request_headers" { DBG_PRINT(READ_REQUEST_HEADERS); return READ_REQUEST_HEADERS; }
"read_response_headers" { DBG_PRINT(READ_RESPONSE_HEADERS); return READ_RESPONSE_HEADERS; }
"send_request_headers" { DBG_PRINT(SEND_REQUEST_HEADERS); return SEND_REQUEST_HEADERS; }
"send_response_headers" { DBG_PRINT(SEND_RESPONSE_HEADERS); return SEND_RESPONSE_HEADERS; }
"server_request" { DBG_PRINT(SERVER_REQUEST); return SERVER_REQUEST; }
"client_request" { DBG_PRINT(CLIENT_REQUEST); return CLIENT_REQUEST; }
"server_response" { DBG_PRINT(SERVER_RESPONSE); return SERVER_RESPONSE; }
"client_response" { DBG_PRINT(CLIENT_RESPONSE); return CLIENT_RESPONSE; }
"header" { DBG_PRINT(HEADER); return HEADER; }
"url" { DBG_PRINT(URL); return URL; }
"scheme" { DBG_PRINT(SCHEME); return SCHEME; }
"path" { DBG_PRINT(PATH); return PATH; }
"port" { DBG_PRINT(PORT); return PORT; }
"query" { DBG_PRINT(QUERY); return QUERY; }
"domain" { DBG_PRINT(DOMAIN); return DOMAIN; }
"if" { DBG_PRINT(IF); return IF; }
"else" { DBG_PRINT(ELSE); return ELSE; }
"==" { DBG_PRINT(EQUAL_EQUAL); return EQUAL_EQUAL; }
"!=" { DBG_PRINT(NOT_EQUAL); return NOT_EQUAL; }
"=~" { DBG_PRINT(EQUAL_TILDE); return EQUAL_TILDE; }
"var" { DBG_PRINT(VAR); return VAR; }

"NOTHING" { DBG_PRINT(NOTHING); return NOTHING; }

({UNDERSCORE}|{LETTER})({UNDERSCORE}|{LETTER}|{DIGIT})* {
	DBG_PRINT(IDENTIFIER); 
 	yylval.v_string=strdup(yytext); 
 	return IDENTIFIER; 
}

\"([^"\\\n])*\" {
 	DBG_PRINT(STRING_LITERAL); 
 	yylval.v_string=strdup(yytext); 
 	return STRING_LITERAL; 
}

(\/[^\*]([^\/]|\\\/)*\/[imsx]*) {
 	DBG_PRINT(REGEX);
 	yylval.v_string=strdup(yytext); 
 	return REGEX; 
}

"[" { DBG_PRINT(OPEN_BRACKET); return OPEN_BRACKET; }
"]" { DBG_PRINT(CLOSE_BRACKET); return CLOSE_BRACKET; }
"{" { DBG_PRINT(OPEN_BRACE); return OPEN_BRACE; }
"}" { DBG_PRINT(CLOSE_BRACE); return CLOSE_BRACE; }
"(" { DBG_PRINT(OPEN_PAREN); return OPEN_PAREN; }
")" { DBG_PRINT(CLOSE_PAREN); return CLOSE_PAREN; }
";" { DBG_PRINT(TERM); return TERM; }
"=" { DBG_PRINT(EQUAL); return EQUAL; }
"+=" { DBG_PRINT(PLUS_EQUAL); return PLUS_EQUAL; }


"\n" { ++yylineno; }
{WS} { /* DBG_PRINT(WS) */ /*whitespace separates tokens */ }
. { DBG_PRINT(.) /* discard bad characters */ }

%%
static void comment(void)
{
    int c;
    while ((c = yyinput()) != 0) {
        if (c == '*') {
            while ((c = yyinput()) == '*') ;

            if (c == '/') return;
            if (c == 0) break;
        }
	}
    yyerror(NULL, "unterminated comment");
 }
