# PiCo

A small, compact scripting language and virtual machine implemented in C. pico includes a compiler, virtual machine, REPL, and a set of core modules for working with values, objects, and I/O.

See the included `manual.md` for a detailed language reference and usage examples: [https://github.com/the0cp/pico/blob/master/manual.md](https://github.com/the0cp/pico/blob/master/manual.md)

## Building

Requirements: gcc and CMake. The code uses specific techniques such as dispatch table, so gcc is necessary. Installing gcc on Windows can be done through several methods, including using MinGW-w64, Chocolatey and MSYS2.

Typical build steps:

```sh
mkdir build
cd build
cmake ..
make
```

The build produces a `pico` executable in the build directory.

## Usage

- Run the interactive REPL:

```sh
./pico
```

- Run a script:

```sh
./pico path/to/script.pcs
```

Check `manual.md` for language syntax, built-in functions, and examples.

## License

This project is distributed under the GNU GPL v3. See `gpl-3.0.txt` for details.
