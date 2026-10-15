#ifndef FLUXMAX_BROWSER_HTML_H
#define FLUXMAX_BROWSER_HTML_H
#include <stddef.h>
typedef enum {
    FLUXMAX_HTML_TEXT, FLUXMAX_HTML_START_TAG, FLUXMAX_HTML_END_TAG,
    FLUXMAX_HTML_COMMENT, FLUXMAX_HTML_DOCTYPE, FLUXMAX_HTML_EOF, FLUXMAX_HTML_ERROR
} fluxmax_html_token_kind_t;
typedef struct { fluxmax_html_token_kind_t kind; const char *start; size_t length; int self_closing; } fluxmax_html_token_t;
typedef struct { const char *input; size_t length; size_t offset; } fluxmax_html_lexer_t;
void fluxmax_html_lexer_init(fluxmax_html_lexer_t *lexer, const char *input, size_t length);
int fluxmax_html_next(fluxmax_html_lexer_t *lexer, fluxmax_html_token_t *token);
#endif
