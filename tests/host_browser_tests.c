#include "browser/browser.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

static char output[8192];
static size_t used;
static void collect(char c, void *ctx) {
    (void)ctx;
    if (used + 1u < sizeof(output)) output[used++] = c;
    output[used] = '\0';
}
static int contains(const char *needle) { return strstr(output, needle) != NULL; }
static int failures;
#define CHECK(cond, name) do { if (cond) printf("[PASS] %s\n", name); else { printf("[FAIL] %s\n", name); ++failures; } } while (0)
int main(void) {
    const char *html = "<!doctype html><html><head><title>Example page</title><style>.x{display:none}</style></head>"
        "<body><h1>Hello &amp; welcome</h1><p>Alpha&nbsp; beta <b>bold</b>.</p>"
        "<script>secret('do not show <b>');</script><ul><li>First</li><li>Second</li></ul>"
        "<p>Numeric &#65; &#x42; &lt;safe&gt;</p></body></html>";
    used = 0; memset(output, 0, sizeof(output));
    CHECK(fluxmax_browser_render_html_text(html, strlen(html), collect, NULL) == 0, "HTML preview accepts a valid document");
    CHECK(contains("Hello & welcome"), "named entities are decoded");
    CHECK(contains("Alpha beta bold."), "inline tags preserve readable text");
    CHECK(!contains("secret") && !contains("display:none"), "script and style contents are suppressed");
    CHECK(contains("* First") && contains("* Second"), "list items are presented as text bullets");
    CHECK(contains("Numeric A B <safe>"), "numeric and angle entities are decoded safely");
    CHECK(fluxmax_browser_render_html_text(NULL, 1u, collect, NULL) != 0, "reject null input with nonzero length");
    CHECK(fluxmax_browser_render_html_text("x", 1u, NULL, NULL) != 0, "reject missing output callback");
    CHECK(fluxmax_browser_render_html_text("x", 1024u*1024u+1u, collect, NULL) != 0, "reject oversized document");
    CHECK(browser_open_url("https://example.com") == -1, "HTTPS URLs are refused until TLS and certificate validation are implemented");
    CHECK(browser_open_url("x") == -1, "short malformed URLs are rejected without reading beyond the string");
    CHECK(browser_open_url("http://") == -1, "incomplete HTTP URLs are rejected without opening a connection");
    printf("Browser preview tests: %d passed, %d failed.\n", 12-failures, failures);
    return failures ? 1 : 0;
}
