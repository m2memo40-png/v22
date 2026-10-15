# Fluxmax text web client

- Shell `browser /path/page.html` reads a local HTML file from the RAM VFS and renders its text.
- Shell `browser http://host/path` fetches an HTTP page through the custom DNS and initial TCP stack.
- Shell `web` prompts for a URL; from the custom desktop, focus/show the Web window and press `b` to type an HTTP URL. The HTTP status/output and HTML text are redirected into the Web window.

The client bounds responses to 128 KiB and parses a subset of status/headers, Content-Length and chunked transfer encoding. The renderer suppresses script/style bodies and decodes selected text entities; it is not a graphical HTML browser.

Unsupported: HTTPS/TLS/certificate verification, JavaScript, CSS layout, images, cookies, authentication, automatic redirects, a complete DOM, tabs, and comprehensive HTTP interoperability. Do not send credentials or sensitive information over HTTP. Transport/browser host tests use simulated TCP peers; live network and GUI behavior still require QEMU/device verification.
