# Embedding examples

These examples show how to use PiCo as an embedded scripting VM from C.

- `basic.c`: create a VM and evaluate PiCo source code.
- `native_function.c`: expose a C function to PiCo.
- `call_script.c`: call a PiCo function from C and let the PiCo function call back into C.

## `basic.c`

Creates a VM, evaluates a PiCo source string, and destroys the VM.

```sh
cmake --build --preset debug --target pico_embed_basic
./build/debug/pico_embed_basic
```

## `native_function.c`

Registers a C function and calls it from PiCo.

```c
pico_vm_register_native(vm, "hostAdd", hostAdd, NULL);
```

PiCo can then call it as a normal function:

```javascript
print hostAdd(20, 22);
```

## `call_script.c`

Loads a PiCo function and calls it from C. The PiCo function calls back into a native C function.

This demonstrates the full embedding loop:

```text
C -> PiCo -> C -> PiCo -> C
```

## `external-cmake/`

A minimal standalone CMake project that links against an installed `libpico.a`.

After installing PiCo into a local prefix, configure it with:

```sh
cmake -S examples/embedding/external-cmake -B build/external-pico -DPICO_ROOT=/path/to/pico/install
cmake --build build/external-pico
```

On Windows PowerShell, for a local debug install:

```powershell
cmake -S .\examples\embedding\external-cmake -B .\build\external-pico -DPICO_ROOT="$PWD\install-debug"
cmake --build .\build\external-pico
.\build\external-pico\embed_app.exe
```

## Source names

The third argument of `pico_vm_eval()` is a diagnostic name:

```c
pico_vm_eval(vm, source, "<script>");
```

It does not need to be a real file. It is used in error messages.

