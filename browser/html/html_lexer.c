#include "browser/html.h"

static int html_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static int starts_with(const char *p, size_t remain, const char *needle, size_t count) {
    if (remain < count) return 0;
    for (size_t i = 0; i < count; ++i) if (p[i] != needle[i]) return 0;
    return 1;
}

void fluxmax_html_lexer_init(fluxmax_html_lexer_t *lexer, const char *input, size_t length) {
    if (!lexer) return;
    lexer->input = input;
    lexer->length = length;
    lexer->offset = 0;
}

int fluxmax_html_next(fluxmax_html_lexer_t *lexer, fluxmax_html_token_t *token) {
    if (!lexer || !token || (!lexer->input && lexer->length)) return -1;
    if (lexer->offset >= lexer->length) {
        token->kind = FLUXMAX_HTML_EOF;
        token->start = lexer->input ? lexer->input + lexer->length : 0;
        token->length = 0;
        token->self_closing = 0;
        return 0;
    }
    const char *base = lexer->input;
    size_t start = lexer->offset;
    size_t remain = lexer->length - start;
    const char *p = base + start;
    if (p[0] != '<') {
        size_t n = 0;
        while (n < remain && p[n] != '<') ++n;
        token->kind = FLUXMAX_HTML_TEXT;
        token->start = p;
        token->length = n;
        token->self_closing = 0;
        lexer->offset += n;
        return 1;
    }
    if (starts_with(p, remain, "<!--", 4)) {
        size_t n = 4;
        while (n + 2 < remain && !starts_with(p + n, remain - n, "-->", 3)) ++n;
        n = n + 2 < remain ? n + 3 : remain;
        token->kind = FLUXMAX_HTML_COMMENT;
        token->start = p;
        token->length = n;
        token->self_closing = 0;
        lexer->offset += n;
        return 1;
    }
    if (starts_with(p, remain, "<!DOCTYPE", 9) || starts_with(p, remain, "<!doctype", 9)) {
        size_t n = 0;
        char quote = 0;
        while (n < remain) {
            char c = p[n++];
            if (quote) { if (c == quote) quote = 0; continue; }
            if (c == '\'' || c == '"') { quote = c; continue; }
            if (c == '>') break;
        }
        token->kind = FLUXMAX_HTML_DOCTYPE;
        token->start = p;
        token->length = n;
        token->self_closing = 0;
        lexer->offset += n;
        return 1;
    }
    size_t n = 1;
    int is_end = 0;
    if (n < remain && p[n] == '/') { is_end = 1; ++n; }
    while (n < remain && html_is_space(p[n])) ++n;
    size_t name_start = n;
    while (n < remain && p[n] != '>' && p[n] != '/' && !html_is_space(p[n])) ++n;
    if (n == name_start) {
        token->kind = FLUXMAX_HTML_TEXT;
        token->start = p;
        token->length = 1;
        token->self_closing = 0;
        lexer->offset++;
        return 1;
    }
    char quote = 0;
    size_t end = n;
    int self_closing = 0;
    while (end < remain) {
        char c = p[end++];
        if (quote) { if (c == quote) quote = 0; continue; }
        if (c == '\'' || c == '"') { quote = c; continue; }
        if (c == '/' && end < remain && p[end] == '>') self_closing = 1;
        if (c == '>') break;
    }
    token->kind = is_end ? FLUXMAX_HTML_END_TAG : FLUXMAX_HTML_START_TAG;
    token->start = p;
    token->length = end;
    token->self_closing = self_closing;
    lexer->offset += end;
    return 1;
}
