#include "browser/browser.h"
#include "browser/html.h"
#include <stddef.h>

static int ascii_lower(int c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
static int html_space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\f'; }
static int name_eq(const char *s, size_t n, const char *target) {
    size_t i=0;
    while (target[i]) { if (i>=n || ascii_lower((unsigned char)s[i]) != target[i]) return 0; ++i; }
    return i==n;
}
static void emit(fluxmax_browser_putc_fn fn, void *ctx, char c) { if (fn) fn(c,ctx); }
static void emit_newline(fluxmax_browser_putc_fn fn, void *ctx, int *line_start) {
    emit(fn,ctx,'\n'); *line_start=1;
}
static int hex_digit(char c) {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
static int decode_entity(const char *s, size_t n, char *out) {
    if (!out) return 0;
    if (n==3 && s[0]=='a'&&s[1]=='m'&&s[2]=='p') { *out='&'; return 1; }
    if (n==2 && s[0]=='l'&&s[1]=='t') { *out='<'; return 1; }
    if (n==2 && s[0]=='g'&&s[1]=='t') { *out='>'; return 1; }
    if (n==4 && s[0]=='q'&&s[1]=='u'&&s[2]=='o'&&s[3]=='t') { *out='"'; return 1; }
    if (n==4 && s[0]=='a'&&s[1]=='p'&&s[2]=='o'&&s[3]=='s') { *out='\''; return 1; }
    if (n==4 && s[0]=='n'&&s[1]=='b'&&s[2]=='s'&&s[3]=='p') { *out=' '; return 1; }
    if (n>1 && s[0]=='#') {
        unsigned value=0; size_t i=1; int base=10;
        if (i<n && (s[i]=='x'||s[i]=='X')) { base=16; ++i; }
        if (i==n) return 0;
        for(;i<n;++i) {
            int d=base==16?hex_digit(s[i]):(s[i]>='0'&&s[i]<='9'?s[i]-'0':-1);
            if(d<0 || (unsigned)d >= (unsigned)base || value > (0x7Fu-(unsigned)d)/(unsigned)base) return 0;
            value=value*(unsigned)base+(unsigned)d;
        }
        *out=value>=32u && value<127u?(char)value:(value==9u?'\t':value==10u?'\n':'?');
        return 1;
    }
    return 0;
}
static void emit_text(const char *s,size_t n,fluxmax_browser_putc_fn fn,void *ctx,
                      int *pending_space,int *line_start) {
    size_t i=0;
    while(i<n) {
        unsigned char c=(unsigned char)s[i++];
        if(c=='&') {
            size_t begin=i, end=i; char decoded=0;
            while(end<n && end-begin<16u && s[end]!=';' && s[end]!='<' && !html_space(s[end])) ++end;
            if(end<n && s[end]==';' && end>begin && decode_entity(s+begin,end-begin,&decoded)) {
                i=end+1u; c=(unsigned char)decoded;
            }
        }
        if(c=='\n' || html_space((char)c)) { *pending_space=1; continue; }
        if(*pending_space && !*line_start) emit(fn,ctx,' ');
        *pending_space=0; emit(fn,ctx,(char)c); *line_start=0;
    }
}
static void tag_name(const fluxmax_html_token_t *t,const char **name,size_t *n,int *closing) {
    const char *p=t->start; size_t len=t->length; *closing=0; *name=p; *n=0;
    if(!p || len<3u) return;
    size_t i=1;
    if(i<len && p[i]=='/') { *closing=1; ++i; }
    while(i<len && html_space(p[i])) ++i;
    size_t start=i;
    while(i<len && !html_space(p[i]) && p[i]!='/' && p[i]!='>') ++i;
    *name=p+start; *n=i-start;
}
static int is_block(const char *s,size_t n) {
    return name_eq(s,n,"p")||name_eq(s,n,"div")||name_eq(s,n,"section")||name_eq(s,n,"article")||
           name_eq(s,n,"header")||name_eq(s,n,"footer")||name_eq(s,n,"main")||name_eq(s,n,"nav")||
           name_eq(s,n,"h1")||name_eq(s,n,"h2")||name_eq(s,n,"h3")||name_eq(s,n,"h4")||
           name_eq(s,n,"h5")||name_eq(s,n,"h6")||name_eq(s,n,"pre")||name_eq(s,n,"tr")||
           name_eq(s,n,"ul")||name_eq(s,n,"ol")||name_eq(s,n,"table")||name_eq(s,n,"blockquote");
}
static fluxmax_browser_url_handler_fn url_handler;
void browser_set_url_handler(fluxmax_browser_url_handler_fn handler) { url_handler=handler; }
int browser_init(void) { return 0; }
static int has_prefix(const char *s,const char *prefix) {
    if(!s || !prefix) return 0;
    while(*prefix) { if(!*s || *s!=*prefix) return 0; ++s; ++prefix; }
    return 1;
}
int browser_open_url(const char *url) {
    if(!url || !*url) return -1;
    if(has_prefix(url,"https://")) return -1;
    if(!has_prefix(url,"http://")) return -1;
    return url_handler?url_handler(url):-1;
}

int fluxmax_browser_render_html_text(const char *html,size_t length,
                                    fluxmax_browser_putc_fn putc_fn,void *context) {
    if((!html && length) || !putc_fn || length > 1024u*1024u) return -1;
    fluxmax_html_lexer_t lexer; fluxmax_html_token_t token;
    fluxmax_html_lexer_init(&lexer,html,length);
    int skip_raw=0, pending_space=0, line_start=1;
    const char *raw_name=0; size_t raw_name_len=0;
    for(;;) {
        int rc=fluxmax_html_next(&lexer,&token);
        if(rc<0) return -1;
        if(rc==0 || token.kind==FLUXMAX_HTML_EOF) break;
        if(token.kind==FLUXMAX_HTML_TEXT) {
            if(!skip_raw) emit_text(token.start,token.length,putc_fn,context,&pending_space,&line_start);
            continue;
        }
        if(token.kind!=FLUXMAX_HTML_START_TAG && token.kind!=FLUXMAX_HTML_END_TAG) continue;
        const char *name; size_t name_len; int closing;
        tag_name(&token,&name,&name_len,&closing);
        if(skip_raw) {
            if(closing && name_eq(name,name_len,raw_name)) { skip_raw=0; raw_name=0; raw_name_len=0; pending_space=1; }
            continue;
        }
        if(!closing && (name_eq(name,name_len,"script") || name_eq(name,name_len,"style"))) {
            skip_raw=1;
            raw_name=name_eq(name,name_len,"script") ? "script" : "style";
            raw_name_len=name_eq(name,name_len,"script") ? 6u : 5u;
            (void)raw_name_len;
            continue;
        }
        if(name_eq(name,name_len,"br") || name_eq(name,name_len,"hr")) {
            pending_space=0; if(!line_start) emit_newline(putc_fn,context,&line_start);
            continue;
        }
        if(name_eq(name,name_len,"li") && !closing) {
            pending_space=0; if(!line_start) emit_newline(putc_fn,context,&line_start);
            emit(putc_fn,context,'*'); emit(putc_fn,context,' '); line_start=0; continue;
        }
        if((is_block(name,name_len) || (closing && (name_eq(name,name_len,"li") || name_eq(name,name_len,"body")))) &&
           !line_start) {
            pending_space=0; emit_newline(putc_fn,context,&line_start);
        }
    }
    return 0;
}
