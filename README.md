# PiCo

A small, compact scripting language and virtual machine implemented in C. pico includes a compiler, virtual machine, REPL, and a set of core modules for working with values, objects, and I/O.

## Features

- Register-based bytecode VM
- REPL
- Functions and closures
- Classes and methods
- Modules
- Lists, maps, strings, and slicing
- Small standard library
- Manual / automatic GC modes

See the included `manual.md` for a detailed language reference and usage examples: [https://github.com/the0cp/pico/blob/master/manual.md](https://github.com/the0cp/pico/blob/master/manual.md)

## Building

Requirements: gcc and CMake. The code uses GCC-specific techniques such as computed goto / dispatch table, so GCC is required. On Windows, GCC can be installed through MinGW-w64, Chocolatey, or MSYS2.

Clone the repo:

```sh
git clone --recursive https://github.com/the0cp/pico.git
```

Configure and build a debug version:

```sh
cmake --preset debug
cmake --build --preset debug
```

Configure and build a release version:

```sh
cmake --preset release
cmake --build --preset release
```

On Windows:

```sh
cmake --preset release-windows
cmake --build --preset release-windows
```

The executable is generated under the corresponding build directory, for example:

```text
build/debug/pico
build/release/pico
build/release-windows/pico.exe
```

## Testing

Run the test suite with *CTest*:

```sh
ctest --preset debug --output-on-failure
```

or for release:

```sh
ctest --preset release --output-on-failure
```

## Usage

Run the interactive REPL:

```sh
./build/release/pico
```

Run a script:

```sh
./build/release/pico path/to/script.pcs
```

For a debug build, use:

```sh
./build/debug/pico path/to/script.pcs
```

Check `manual.md` for language syntax, built-in functions, and examples.

## License

This project is distributed under the GNU GPL v3. See `gpl-3.0.txt` for details.
