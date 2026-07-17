# PiCo → Cieto Rename Plan

## Objective

Rename the existing **PiCo** programming language and virtual machine project to **Cieto** without changing runtime behavior, language semantics, bytecode semantics, performance characteristics, or architecture.

This must be a **pure naming refactor**.

The final public naming scheme is:

| Surface                   | Old                                      | New                    |
| ------------------------- | ---------------------------------------- | ---------------------- |
| Project / language        | PiCo / Pico / pico                       | Cieto / cieto          |
| CLI executable            | `pico`                                   | `cieto`                |
| Source extension          | `.pcs`                                   | `.cies`                |
| Library                   | `libpico`                                | `libcieto`             |
| Public type prefix        | `Pico...` / project-specific equivalent  | `Cie...`               |
| Public function prefix    | `pico_...` / project-specific equivalent | `cie_...`              |
| Public macro prefix       | `PICO_...`                               | `CIE_...`              |
| Installed header          | `pico.h` or equivalent                   | `cieto.h`              |
| Environment/config prefix | `PICO_...`                               | `CIETO_...`            |
| Shebang                   | `#!/usr/bin/env pico`                    | `#!/usr/bin/env cieto` |

Brand rationale:

> **Cieto** is inspired by the Latin verb *cieō*, “to set in motion.”

Canonical project description:

> **A compact C-family scripting language and embeddable register-based virtual machine implemented entirely in C.**

---

# Critical Constraints

1. **Do not change behavior.**
   
   - Do not alter syntax.
   
   - Do not alter parser rules.
   
   - Do not alter opcode values or layouts.
   
   - Do not alter bytecode encoding.
   
   - Do not alter GC behavior.
   
   - Do not alter module semantics.
   
   - Do not alter object layouts.
   
   - Do not alter public function behavior.
   
   - Do not perform unrelated cleanup or optimization.

2. **Keep the rename isolated.**
   
   - Use a dedicated branch such as:
     ```bash
     git switch -c refactor/rename-to-cieto
     ```
   
   - Produce one focused commit:
     ```text
     refactor: rename PiCo to Cieto
     ```

3. **Do not create, delete, or modify unrelated documentation.**
   
   - Do not create `systems_interview_notes.md`.
   
   - Do not delete unrelated files under `docs/`.
   
   - Do not include unrelated formatting changes.
   
   - Do not reorganize directories unless the rename itself requires it.

4. **Do not use blind global replacement.**
   
   - Inspect every category first.
   
   - Avoid changing unrelated words containing `pico`.
   
   - Avoid altering third-party dependency names, URLs, licenses, or historical author names unless they specifically refer to this project.

5. **Do not add compatibility aliases by default.**
   
   - This is intended to be a clean rename.
   
   - Do not keep `pico_*`, `Pico*`, `PICO_*`, the `pico` executable, or `.pcs` aliases unless the repository already has a documented compatibility policy requiring them.

6. **Preserve project requirements.**
   
   - C11 plus the GNU extensions already used by the project.
   
   - GCC remains the required compiler unless the existing build already supports others.
   
   - Preserve the computed-goto interpreter and all existing low-level implementation choices.

---

# Phase 1: Inventory Before Editing

Start from a clean worktree:

```bash
git status --short
```

If the worktree is not clean, stop and report the unrelated changes instead of overwriting them.

Inspect the repository structure and existing build/test commands:

```bash
find . -maxdepth 3 -type f | sort
```

Search all current branding and extensions:

```bash
rg -n --hidden \
  --glob '!.git/**' \
  'PiCo|Pico|PICO|pico|\.pcs|libpico|pico\.h|PICO_PATH|#!/usr/bin/env pico'
```

Also inspect filenames and directories:

```bash
find . -depth \
  \( -iname '*pico*' -o -iname '*.pcs' \) \
  -print
```

Classify every match into one of these groups before editing:

1. Public C API
2. Internal C identifiers
3. CLI executable and entry point
4. Build-system targets
5. Installation paths
6. Source-file extension handling
7. Tests and fixtures
8. Examples and benchmarks
9. Documentation
10. CI/release/package configuration
11. Comments, diagnostics, and version strings
12. Historical or unrelated references that must remain unchanged

Write a short internal checklist from the actual repository contents. Do not invent files or APIs that do not exist.

---

# Phase 2: Apply the Canonical Naming Scheme

## 2.1 Project and CLI Branding

Rename user-facing forms consistently:

```text
PiCo  -> Cieto
Pico  -> Cieto
pico  -> cieto
```

This includes, where present:

- README title
- CLI help text
- version output
- startup banners
- error prefixes
- package descriptions
- CMake project name
- Makefile target names
- install targets
- release archive names
- CI artifact names
- example commands
- shell completions
- man pages

Expected CLI examples:

```bash
cieto main.cies
cieto --dump examples/fibonacci.cies
cieto --version
cieto --help
```

The executable installed into `PATH` must be named:

```text
cieto
```

Do not leave a second `pico` executable unless compatibility is explicitly requested later.

---

## 2.2 Source Extension

Rename the language source extension:

```text
.pcs -> .cies
```

Rename all tracked source programs, including:

- examples
- tests
- benchmarks
- module fixtures
- sample scripts
- documentation snippets
- CI commands

Use `git mv`, not delete-and-recreate:

```bash
git mv path/to/example.pcs path/to/example.cies
```

Update all extension-sensitive code:

- command-line validation
- module resolution
- script import resolution
- default file extension constants
- error messages
- syntax-highlighting files
- editor integration
- test discovery
- packaging manifests

Examples:

```text
examples/fibonacci.cies
tests/test_gc_mode.cies
tests/test_modules.cies
benchmarks/arithmetic_loop.cies
```

Update shebangs:

```text
#!/usr/bin/env cieto
```

Do not change the syntax inside `.cies` files.

---

## 2.3 Public C API

The public API must use the `Cie` / `cie_` / `CIE_` namespace.

Map actual existing public names according to these rules:

```text
PicoVM          -> CieVM
PicoValue       -> CieValue
PicoResult      -> CieResult
PicoConfig      -> CieConfig
PicoNativeFn    -> CieNativeFn

pico_*          -> cie_*
PICO_*          -> CIE_*
```

The exact list must come from the real public headers.

Illustrative target style:

```c
typedef struct CieVM CieVM;

typedef enum {
    CIE_OK,
    CIE_ERR_COMPILE,
    CIE_ERR_RUNTIME
} CieResult;

CieVM *cie_vm_new(const CieConfig *config);
void cie_vm_free(CieVM *vm);

CieResult cie_eval(CieVM *vm, const char *source);
CieResult cie_run_file(CieVM *vm, const char *path);
```

Naming rules:

- Public opaque types: `Cie...`
- Public functions: `cie_...`
- Public macros and enum constants: `CIE_...`
- No exported `pico_*`, `Pico*`, or `PICO_*` symbols after the rename
- Preserve signatures, ABI-relevant layouts, calling conventions, and behavior except for symbol names

Header naming:

- If the project currently installs a flat public header, rename it to:
  
  ```text
  cieto.h
  ```
- If the project already uses a namespaced include directory, use:
  
  ```c
  #include <cieto/cieto.h>
  ```
- Do not introduce a new include-directory architecture solely for this rename.

Update:

- header guards
- export macros
- pkg-config files
- CMake package config
- installed include paths
- examples embedding the VM
- tests of the embedding API

Example header guard:

```c
#ifndef CIETO_H
#define CIETO_H

/* ... */

#endif
```

---

## 2.4 Internal C Identifiers

Rename internal identifiers only when they encode the old project brand.

Examples:

```text
picoMain       -> cietoMain
picoVersion    -> cietoVersion
PICO_VERSION   -> CIETO_VERSION
```

Do not rename functional internal terms that do not contain project branding.

Do not perform broad style changes such as:

- changing camelCase to snake_case
- restructuring static functions
- renaming generic VM fields
- splitting source files
- moving modules
- rewriting macros

Static internal symbols that are unrelated to the brand should remain unchanged.

---

## 2.5 Build System

Inspect the actual build system before editing.

Rename, where present:

```text
pico executable target       -> cieto
pico library target          -> cieto or libcieto
libpico.a                     -> libcieto.a
libpico.so                    -> libcieto.so
pico.exe                      -> cieto.exe
```

Update:

- `CMakeLists.txt`
- Makefiles
- build scripts
- installation scripts
- pkg-config metadata
- CI workflow commands
- release packaging
- Windows target names
- test commands
- benchmark commands

Preserve compiler flags and implementation requirements.

After installation, these should resolve to the new names:

```bash
command -v cieto
cieto --version
```

The old binary should not remain installed accidentally.

If the project supports `make install`, verify installation into a temporary prefix first:

```bash
make DESTDIR=/tmp/cieto-install install
find /tmp/cieto-install -type f | sort
```

Adapt this command to the actual build system rather than inventing a new install flow.

---

## 2.6 Modules and Script Resolution

Inspect code responsible for:

- native module registration
- script module loading
- `import`
- source-path resolution
- default extension insertion
- search paths
- diagnostics

Change only branding and extension data:

```text
.pcs -> .cies
PICO_PATH -> CIETO_PATH
```

If an environment variable like `PICO_PATH` exists, rename it to:

```text
CIETO_PATH
```

Do not change:

- import scope
- module caching
- native-module lookup
- path normalization behavior
- script-vs-native resolution order
- GC ownership

Add or update tests proving that imports using `.cies` behave exactly as `.pcs` did before the rename.

---

## 2.7 Bytecode and Dump Output

The project already supports or is developing `--dump`.

Update branding and example filenames:

```bash
cieto --dump examples/fibonacci.cies
```

Do not change:

- opcode values
- instruction formats
- register indexing
- constant-pool behavior
- disassembly semantics
- closure representation
- serialized-format design

If a speculative compiled artifact such as `.pco` is mentioned only in notes and is not implemented, do not invent or rename it during this task.

If an implemented artifact format exists, report it separately before changing its extension.

---

## 2.8 Documentation

Update branding in:

- README
- build instructions
- installation instructions
- embedding examples
- benchmark commands
- module examples
- release notes
- comments describing the project
- badges and artifact labels

Recommended README opening:

```markdown
# Cieto

A compact C-family scripting language and embeddable register-based virtual machine implemented entirely in C.

Cieto is inspired by the Latin verb *cieō*, “to set in motion.”
```

Example usage:

```bash
cieto examples/fibonacci.cies
```

Embedding example:

```c
#include <cieto.h>

int main(void) {
    CieVM *vm = cie_vm_new(NULL);
    if (vm == NULL) {
        return 1;
    }

    CieResult result = cie_run_file(vm, "main.cies");
    cie_vm_free(vm);

    return result == CIE_OK ? 0 : 1;
}
```

Adapt function names to the real API; do not create APIs that the project does not currently provide.

Historical references may remain when explicitly describing the rename, for example:

```text
Cieto was previously named PiCo.
```

Do not leave PiCo as the current project name anywhere else.

---

# Phase 3: Validation

## 3.1 Search for Missed Branding

Run all of the following after editing:

```bash
rg -n --hidden \
  --glob '!.git/**' \
  'PiCo|Pico|PICO|pico|\.pcs|libpico|pico\.h|PICO_PATH|#!/usr/bin/env pico'
```

Review every remaining match manually.

Acceptable remaining matches are limited to:

- an intentional migration note
- immutable historical references
- unrelated third-party text
- Git metadata outside the worktree

Do not simply suppress remaining matches.

Also check filenames:

```bash
find . -depth \
  \( -iname '*pico*' -o -iname '*.pcs' \) \
  -print
```

Expected result: no project-owned current files.

---

## 3.2 Build and Test

Use the repository's existing documented commands and CI configuration.

At minimum, validate:

1. Debug build
2. Release build
3. Existing automated tests
4. Script execution
5. Module imports
6. Closures
7. Classes
8. GC mode tests
9. Iterator and slicing tests
10. `--dump`
11. C embedding API
12. Installation
13. Windows/MinGW build if already supported
14. Existing benchmark programs as smoke tests

Representative smoke tests:

```bash
./build/debug/cieto examples/fibonacci.cies
./build/release/cieto examples/fibonacci.cies
./build/release/cieto --dump examples/fibonacci.cies
```

Adapt paths to the actual build output.

Run all existing test scripts after renaming them to `.cies`.

The rename must not change expected output except for intentional project-name or filename text.

---

## 3.3 Public Symbol Audit

If a library is built, inspect exported symbols:

```bash
nm -g --defined-only path/to/libcieto.a | sort
```

For a shared library, use the platform-appropriate symbol inspection tool.

Verify:

- expected `cie_*` public symbols exist
- no old exported `pico_*` symbols remain
- internal symbols did not unintentionally become public
- symbol signatures remain equivalent

---

## 3.4 Runtime String Audit

Inspect the built binary for stale user-facing branding where practical:

```bash
strings path/to/cieto | rg 'PiCo|Pico|PICO|pico|\.pcs'
```

Review every match.

Do not treat unrelated compiler paths or embedded source paths as automatic failures, but remove stale project branding from user-facing output.

---

## 3.5 Behavior Comparison

Before the rename, capture representative outputs if possible:

- Fibonacci example
- arithmetic benchmark result
- module tests
- GC mode test
- class/closure test
- bytecode dump

After the rename, compare them.

Only these differences are expected:

```text
pico -> cieto
PiCo -> Cieto
.pcs -> .cies
public C API symbol names
```

No opcode, runtime result, error classification, benchmark algorithm, or semantic output should change.

---

# Phase 4: Git Review

Check the final diff:

```bash
git status --short
git diff --stat
git diff --check
git diff
```

The diff should consist only of:

- file renames
- identifier renames
- build/install target renames
- extension updates
- user-facing branding updates
- directly related tests and documentation

Reject and revert:

- unrelated refactors
- whitespace-only rewrites
- mass formatter output
- architecture changes
- performance changes
- new features
- removed unrelated documentation
- speculative compatibility layers

Commit only after all tests pass:

```bash
git add -A
git commit -m "refactor: rename PiCo to Cieto"
```

Do not push or rename the GitHub repository unless explicitly requested.

---

# Acceptance Criteria

The rename is complete only when all conditions are true:

- [ ] Project is branded **Cieto**
- [ ] CLI executable is `cieto`
- [ ] Source files use `.cies`
- [ ] Public C types use `Cie...`
- [ ] Public C functions use `cie_...`
- [ ] Public macros/constants use `CIE_...`
- [ ] Library is named `libcieto`
- [ ] Installed header uses the Cieto name
- [ ] Shebangs use `cieto`
- [ ] Module resolution recognizes `.cies`
- [ ] Existing tests pass
- [ ] Debug and release builds pass
- [ ] `--dump` works with `.cies`
- [ ] Installation produces the expected Cieto files
- [ ] Windows build still passes if previously supported
- [ ] No unintended `pico`, `PiCo`, `Pico`, `PICO`, or `.pcs` references remain
- [ ] No runtime semantics changed
- [ ] No bytecode semantics changed
- [ ] No unrelated files were modified
- [ ] No `systems_interview_notes.md` file was created
- [ ] The work is contained in one focused rename commit

---

# Final Report Required from Codex

After completing the work, report:

1. The files and targets renamed
2. The public API mapping applied
3. The source-extension changes
4. The build and test commands run
5. The results of each validation step
6. Any intentionally retained old-name references and why
7. Any items that require manual GitHub-side changes

Do not merely say “rename complete.” Include concrete evidence from the build, tests, searches, and final diff.
