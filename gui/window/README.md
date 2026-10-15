# Custom window manager

Implementation: `../window_manager.c`, public API: `../../include/gui/window_manager.h`; integrated into `../desktop.c` and the kernel build.

The state model manages four named windows: System Monitor, File Manager, Network Status, and Fluxmax Web. Supported operations include show/focus, cycling focus, clamped move and resize, minimize, maximize/restore, close and reopen. The UI is keyboard-driven: Tab/n/p focus; h/j/k/l move; +/- resize; m minimize; z maximize/restore; x close; numeric keys reopen/select; q exits the desktop. The Web window captures the plain-HTTP text renderer output.

Host model tests are run by `make test-wm`. These tests validate state transitions/geometry; they do not prove pixel rendering or end-to-end guest input. No mouse, pointer hit-testing, drag-to-move, independent userspace GUI surfaces, or full application framework is implemented yet.
