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

## What it looks like

```javascript
# A tiny PiCo demo: 

func slug(s) {
    return s.trim().lower().replace(" ", "-");
}

func badge(s) {
    return "[" + s + "]";
}

func makeCounter(prefix) {
    var n = 0;

    return func(name) {
        n++;
        return "${prefix}-${n}: ${name}";
    };
}

var next = makeCounter("demo");
var topics = [" Register VM ", " Pipe Operator ", " Path Join "];

for (var topic : topics) {
    var name = topic |> slug |> badge;
    print next(name);
}

print "path: ${"examples" / "data" / "sample.txt"}";
print "slice: ${"register-vm"[0:8]}, reverse: ${"PiCo"[::-1]}";

$> echo hello from the host shell
print "shell exit code = ${_exit_code}";
```

## Examples

More examples are available in `examples/`.

Try more examples:

```sh
./build/debug/pico examples/tour.pcs
./build/debug/pico examples/file_indexer.pcs
./build/debug/pico examples/modules/main.pcs
# ...
```

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

## Installing

Install PiCo to your local user prefix:

```sh
cmake --preset release
cmake --build --preset release
cmake --install build/release --prefix ~/.local
```

Make sure `~/.local/bin` is in your PATH:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

Then run PiCo from anywhere.

To install system-wide:

```sh
sudo cmake --install build/release
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
pico
```

Run a script:

```sh
pico path/to/script.pcs
```

Pico scripts can also be executed directly with a Unix shebang:

```sh
#!/usr/bin/env pico

print("hello from Shebang");
```

Make it executable and run it:

```sh
chmod +x hello.pcs
./hello.pcs
```

Check `manual.md` for language syntax, built-in functions, and examples.

## License

This project is distributed under the GNU GPL v3. See `gpl-3.0.txt` for details.
