---
name: nasa-review
description: Review code against NASA JPL's Power of Ten safety-critical coding rules. Use when the user says "nasa review", "nasa rules", "power of ten", or wants a safety-critical code audit.
---

# NASA Power of Ten Code Review

Review the target code against all 10 of NASA JPL's Power of Ten rules (Gerard Holzmann). Produce a structured report with specific findings and file:line citations.

## Target Resolution

**If an argument was provided** (file path or directory): use it.

**If no argument**: default to all three firmware directories:
- `tracker_echo/`
- `tracker_foxtrot/`
- `tracker_delta/` (if it exists)

## File Discovery

Use Glob to find all `.ino` and `.h` files in the target. Read each file fully before beginning the review. For JS targets (`.js`, `.html`), note which rules are N/A.

## The 10 Rules

Evaluate each rule in order. For each, output:
- Rule number, name, and one-sentence definition
- **Status**: PASS / WARN / FAIL
- **Findings**: specific violations with `[file:line](file#Lline)` links — or "No violations found."
- **Recommendation**: one sentence on what to fix (only if WARN or FAIL)

---

### Rule 1 — Simple Control Flow
> No `goto`, `setjmp`/`longjmp`, or direct/indirect recursion.

Search for: `goto`, `setjmp`, `longjmp`, recursive function calls (a function that calls itself, or a call cycle A→B→A).

*JS applicability*: check for recursion only; no `goto` in JS.

---

### Rule 2 — Bounded Loops
> Every loop must have a fixed upper bound that a static checker can verify.

Search for: `while(true)`, `for(;;)`, `while(1)`, `do { ... } while` without a visible counter or termination condition. Flag any loop where the bound depends on external data or a non-constant condition with no fallback limit.

*JS applicability*: same.

---

### Rule 3 — No Dynamic Memory After Init
> No `malloc`, `free`, `realloc`, `new`, or `delete` after the initialization phase (after `setup()` returns in Arduino).

Search for: `malloc(`, `calloc(`, `realloc(`, `free(`, `new `, `delete ` — flag any that appear inside `loop()` or functions called from `loop()`. Calls inside `setup()` or constructors are WARN (acceptable but noted).

*JS/Node applicability*: N/A — mark as N/A and skip.

---

### Rule 4 — Short Functions
> No function longer than ~60 lines (what fits on one printed page at standard formatting).

Count lines between the opening `{` and closing `}` of each function definition. Flag any function exceeding 60 lines. List function name, file, start line, and line count.

*JS applicability*: same threshold.

---

### Rule 5 — Assertion Density
> At least 2 assertions per non-trivial function (functions with >5 lines of logic).

Search for `assert(`, `static_assert(`, `ESP_ERROR_CHECK(` in C++ files. For JS, count `console.assert(`. Flag functions with meaningful logic (>5 lines) but fewer than 2 assertions. List violating function names with file:line.

---

### Rule 6 — Minimal Variable Scope
> Declare all data objects at the smallest possible scope — inside the block where they're used, not at file or global scope unless necessary.

Search for: global or file-scope variables (outside any function) that are only used in a single function. Flag unnecessary globals. List variable name, declaration site, and usage scope.

*JS applicability*: same — flag `var`/`let`/`const` at module scope used only in one function.

---

### Rule 7 — Check Return Values and Parameters
> Every non-void function's return value must be checked by the caller. Every function must validate its parameters.

Search for:
- Calls to non-void functions (e.g., `Serial.begin(`, `WiFi.begin(`, `SD.begin(`, `client.connect(`, `file.open(`, `xTaskCreate(`) where the return value is discarded (call statement not assigned to a variable or wrapped in a condition).
- Functions that accept pointer or reference parameters without a null/validity check at the top.

Flag each unchecked call with file:line.

---

### Rule 8 — Preprocessor Discipline
> Preprocessor use is limited to `#include` and simple `#define` constants. No conditional compilation (`#ifdef`/`#if`), token pasting, variadic macros, or multi-statement macros.

Search for: `#ifdef`, `#ifndef`, `#if `, `#elif`, `##` (token paste), `__VA_ARGS__`. Flag each with file:line.

*JS/Node applicability*: N/A — mark as N/A and skip.

---

### Rule 9 — Pointer Restrictions
> Limit pointer use to a single level of dereference. No function pointers.

Search for:
- Double (or deeper) pointer declarations: `**` in parameter lists or variable declarations.
- Function pointer declarations: e.g., `void (*fn)(`, `typedef ... (*...)(`.
- Pointer arithmetic beyond simple array indexing.

*JS/Node applicability*: N/A — mark as N/A and skip.

---

### Rule 10 — Zero Compiler Warnings
> Compile with all warnings enabled; zero warnings in release builds.

Check `build.sh` (for Echo) and the Foxtrot arduino-cli commands for `-Wall`, `-Wextra`, or equivalent flags. Note whether warning suppression pragmas (`#pragma GCC diagnostic ignore`) appear in source files.

*JS applicability*: check for ESLint config or equivalent. If absent, note it.

---

## Output Format

After evaluating all 10 rules, output:

```
## Summary

| # | Rule | Status |
|---|------|--------|
| 1 | Simple Control Flow | PASS |
| 2 | Bounded Loops | WARN |
...
```

Then a short **Priorities** section: list the top 3 most impactful violations to fix first, ordered by severity.

Keep rule-level findings concise. Use `[file:line](file#Lline)` link format for all citations so they are clickable in VS Code.
