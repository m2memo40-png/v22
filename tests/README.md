# Host tests

Run all suites with `make test` (122 checks in the v21 source tree). The window-manager tests exercise geometry and state operations (focus/cycle/show/close/move/resize/minimize/maximize), not framebuffer rendering after boot. HTTP tests use a simulated TCP peer and also assert that response output can be redirected to the custom Web window sink. Network and TCP tests are not substitutes for real RTL8139/E1000 DMA or QEMU/device integration.
