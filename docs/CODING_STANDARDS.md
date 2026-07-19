# Coding standards

Conventions used across the AXION codebase. The goal is a single, predictable
style so any file reads like the others.

## Language

- All source code, identifiers, comments, log strings and English docs are in
  **English**.
- The only Portuguese document is `docs/GUIA_DO_DESENVOLVEDOR.md` (developer
  guide), by request.

## Firmware (C, ESP-IDF)

### Files

- One module = one `.h` + one `.c` (e.g. `display.h` / `display.c`).
- Headers carry all documentation; implementation files carry none.

### Comments

- **`.h` files:** Doxygen only. Every public function, type and macro has a
  Doxygen block. No free-floating prose.
- **`.c` files:** **no comments at all.** The code is kept small and named
  clearly; the rationale lives in the header Doxygen and in the developer
  guide.

### Doxygen template

```c
/**
 * @brief One-line summary.
 *
 * Optional longer description.
 *
 * @param  name   What it is.
 * @param[out] name  Output parameter.
 * @return  What comes back and the meaning of edge values.
 */
```

Each header also opens with a `@file` block describing the module.

### Naming

| Kind | Convention | Example |
|---|---|---|
| Function | `module_snake_case` | `display_draw_glyph`, `ble_scan_init` |
| Type | `snake_case_t` | `rssi_snapshot_t`, `net_state_t` |
| Enum constant | `MODULE_UPPER_SNAKE` | `NET_ONLINE`, `NET_OFFLINE` |
| File-scope static var | `s_` + `snake_case` | `s_strip`, `s_whitelist_len` |
| Local / parameter | `snake_case` | `now_ms`, `mac_s` |
| Shared config macro | `AXION_UPPER_SNAKE` | `AXION_LED_GPIO` |
| File-local macro | `UPPER_SNAKE` | `MAX_BEACONS`, `WHITELIST_MAX` |

### Formatting

- 4-space indentation, no tabs.
- Braces on the same line as the statement (K&R).
- One statement per line.
- All shared tunables live in `axion_config.h`; do not hardcode magic numbers
  elsewhere.

## Server (Python, FastAPI)

- `snake_case` for functions, variables and modules; `PascalCase` for classes;
  `UPPER_SNAKE` for constants.
- Module docstring at the top of every file; short docstrings on non-obvious
  functions. Inline comments are allowed but sparing and in English.
- Type hints on request/response models (Pydantic).
- Constants live in `app/config.py`.

## Web (HTML/CSS/JS)

- Shared styles in `static/app.css`; pages link it, no inline `<style>`.
- Plain vanilla JS, no build step and no external dependencies (offline-first).
- CSS custom properties (`--zone-a`, etc.) hold the palette.
