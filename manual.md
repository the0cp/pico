# Manual

PiCo source files generally use the `.pcs` extension. The language syntax is inspired by the C/JavaScript family.

## Statements

Statements must be terminated with a semicolon `;`.

## Comments

PiCo supports two types of comments:

- **Single-line comments**: Start with `#` and continue to the end of the line.

- **Block comments**: Start with `#{` and end with `}#`. A key feature is that block comments can be nested.

Examples:

```
# This is a single-line comment

#{ 
   This is a block comment.
   #{ Nested comments are supported }#
}#
```

## Data Types

PiCo is dynamically typed. It supports the following fundamental types:

- **null**: Represents the absence of a value. Keyword: `null`.

- **boolean**: Logical values. Keywords: `true`, `false`.

- **number**: Double-precision floating-point numbers (C `double`).

- **string**: Immutable sequence of characters.

- **list**: Dynamic-typed array/list.

- **object**: Complex types including functions , modules, classes, instances, files, etc..

## Literals & Variables

### Strings and Interpolation

Strings are enclosed in double quotes `"`. 

PiCo supports interpolation, allowing expressions to be embedded directly within string literals. Supports standard escapes like `\n`, `\t`, `\r`, `\"`, `\\`, and `\$`.

Example:

```
var name = "PiCo";
print "Hello, ${name}!"; 
# Output: Hello, PiCo!
```

### Lists

Lists are defined using square brackets `[]`.

- **Standard Definition**: `var list = [1, 2, "three"];`.

- **Bulk Fill Initialization**: `[value; count]` creates a list of `count` elements, all initialized to `value`.

Example:

```
var zeros = [0; 5]; # Creates [0, 0, 0, 0, 0]
```

### Maps

Maps are key-value pairs enclosed in curly braces `{}`.

- **Syntax**: `{ key1: value1, key2: value2 }`.
- **Access**: Values are accessed using square brackets `[]` with the key.

> Map keys must be integers. Using a float with a fractional part (e.g., `1.5`) as a key will result in a runtime error. However, `1.0` is treated as integer `1`.

Example:

```
var dict = { 
    "name": "PiCo", 
    "version": 1, 
    true: "Verified" 
};

print dict["name"]; # Output: PiCo 
dict["new_key"] = 100;
```

### Variable Declaration

Variables are declared using the `var` keyword.

- Variables are local to their scope unless defined at the top level.

- If declared without an initial value, they default to `null`.

Example:

```
var x = 10;
var y;      
# y is null
```

## Operators

PiCo supports standard operators with precedence rules defined in the compiler.

### Arithmetic

- `+` (Addition), `-` (Subtraction), `*` (Multiplication), `/` (Division), `%` (Modulo).

- Increment & Decrement: `++`, `--`. 

Prefix (`++i`, `--i`): Increments/Decrements the value and returns the new value. Only supported for variables.

Postfix (`i++`, `i--`): Increments/Decrements the value and returns the original value. Supported for variables, object properties, and list indices.

Example:

```
var a = 15;
print ++a;      # Output: 16 (a is 16)
print a++;      # Output: 16 (a becomes 17)

class Box { 
  Val = 0;
}

var obj = Box();
obj.Val++;      # Supported (Postfix)

var list = [10, 20];
list[0]++;      # Supported (Postfix), list[0] becomes 11
```

- **String Concatenation**: The `+` operator is overloaded to concatenate strings if operands are strings.

- **Unary**: `-` (Negation).

### Assignment & Increment
PiCo provides shorthand operators for modifying variables and properties.

- Compound Assignment: `+=`, `-=`.

  - `a += b` is equivalent to `a = a + b`.

  - `a -= b` is equivalent to `a = a - b`.

### Implicit String Concatenation

If either operand of the `+` operator is a String, the other operand is automatically converted to a String, and the result is their concatenation.

Example:

```
var age = 20;
print "Age: " + age;  # Output: "Age: 20"
print 100 + "%";      # Output: "100%"
```

### Path Concatenation

When the division operator `/` is used with two strings, it functions as a platform-independent path joiner.

Example:

```
var path = "users" / "docs" / "report.txt";
# Linux/Mac: "users/docs/report.txt"
# Windows:   "users\docs\report.txt"
```

### Comparison

- `==`, `!=`, `<`, `>`, `<=`, `>=`.

- Comparisons return `true` or `false`.

> Lists are compared by value, not by reference. Two separate list objects containing the same elements in the same order are considered equal. `[1, 2] == [1, 2]; # true`


### Logic

- `and`: Logical AND (short-circuit evaluation).

- `or`: Logical OR (short-circuit evaluation).

- `!`: Logical NOT.

### Indexing & Slicing

PiCo supports powerful indexing and slicing operations for both **Lists** and **Strings**.

#### Indexing

Access a single element using square brackets `[]`.

- **Syntax**: `list[index]`.

- **Negative Indexing**: Negative integers index from the end of the list (e.g., `list[-1]` accesses the last item).

Example:

```
var list = [10, 20, 30]; 
print list[0]; # 10 
print list[-1]; # 30
```

#### Slicing

Extract a subsequence using the slice operator `:`. This creates a new string or list.

- **Syntax**: `sequence[start : end : step]`

- **Parameters**:
  - `start` (optional): The starting index (inclusive). Defaults to `0` (or end if step is negative).
  - `end` (optional): The ending index (exclusive). Defaults to length (or beginning if step is negative).
  - `step` (optional): The increment size. Defaults to `1`.

- **Behavior**:
  - **Clamping**: Out-of-bounds indices are automatically clamped to valid ranges. No errors are thrown for safe slicing.
  - **Negative Step**: If `step` is negative, the sequence is traversed in reverse.

Examples:

```
var s = "Hello World";

# Basic Slicing
print s[0:5]; # "Hello" 
print s[6:]; # "World" (6 to end) 
print s[:5]; # "Hello" (Beginning to 5)

# Negative Indices
print s[-5:]; # "World"

# Step & Reversal
print s[::2]; # "HloWrd" (Every 2nd char) 
print s[::-1]; # "dlroW olleH" (Reverse string)

# Lists work the same way
var list = [1, 2, 3, 4, 5]; 
print list[1:4]; # [2, 3, 4]
```

## Control Flow & Statements

### Truthy and Falsy Values

In conditional statements (like `if` or `while`), values are evaluated as follows:

- **Falsy**: `false`, `null`, and the number `0`.

- **Truthy**: All other values (including empty strings `""` and empty lists `[]`).

### if-else

The `if` statement executes a block of code if a condition is `true`. An optional `else` block can be provided.

- Example:

```
if(condition){
    # body
}else{
    # else body
}
```

- **Parentheses**: The condition must be enclosed in parentheses `()`.

- **Scope**: Blocks `{}` create a new scope.

### Loops

PiCo supports `while` and C-style `for` loops. Both support `break` and `continue`.

#### while

Repeats a block of code as long as the condition is true.

```
while(condition){
    # body
}
```

#### for

Standard C-style for loop with initialization, condition, and increment clauses.

```
for(var i = 0; i < 10; i = i + 1){
    print i;
}
```

All three clauses are optional (e.g., `for (;;) { ... }` creates an infinite loop).

#### foreach

PiCo supports a simplified syntax for iterating over lists using the `:` operator in a for loop.

- **Syntax**: `for (var item : list) { ... }`

Example:

```
var nums = [10, 20, 30];
for (var n : nums) {
   print n;
}

#{
   output:
   10
   20
   30
}#
```

#### break & continue

- `break`: Exits the current nearest loop immediately.

- `continue`: Skips the rest of the loop body and jumps to the next iteration (or the increment clause in a `for` loop).

### Switch Statement

PiCo provides a `switch` statement for matching a value against multiple possibilities.

Example:

```
switch(value){
    case 1, 2 => {
        print "One or Two";
    }
    case 3 => print "Three";
    default => print "Other";
}
```

- **Arrow**: Uses `=>` to separate the case value from the body.

- **Multiple Matches**: A single `case` can match multiple values separated by commas (e.g., `case 1, 2`).

- **No Fallthrough**: Unlike C, PiCo switches **do not** fall through automatically. You do not need to write `break`.

- **Default**: An optional `default` case handles values not matched by any case.

### Command Statement

PiCo features a simple syntax for executing system shell commands directly.

- **Syntax**: `$> command`

- **Behavior**:
  
  1. Pauses PiCo execution.
  
  2. Passes the rest of the line (after `$>` ) to the underlying system shell.
  
  3. Prints the command's output directly to the standard output (stdout).
  
  4. This is a statement, not an expression. The exit code is discarded in the current implementation (verified via `OP_POP` in compiler).

Example:

```
print "Listing files:";
$> ls -la
print "Done.";
```

## Functions

Functions in PiCo are first-class citizens (internally). Current syntax supports function declarations primarily as statements. They support closures, capturing variables from their enclosing scope.

### Function Declaration

Use the `func` keyword to define a function.

```
func add(a, b){
    return a + b;
}
```

The `return` keyword exits the function with a value. If no value is returned, it implicitly returns `null`.

### Closures

Functions can access variables defined in their outer scopes (by upvalues).

```
func makeCounter(){
    var count = 0;
    func increment(){
        count = count + 1;
        return count;
    }
    return increment;
}

var counter = makeCounter();
print counter(); # 1
print counter(); # 2
```

## Class & Objects

PiCo implements a unique flavor of Object-Oriented Programming (OOP) that separates **data definition** (classes) from **behavior definition** (methods), similar to Go or Rust's approach, but with explicit class definitions for fields.

### Class Definition (Fields)

The `class` block is strictly for defining data structures (fields) and their default values.

```
class Point{
    x = 0;
    y = 0;
    name; # Defaults to null
}
```

Call the class name like a function to create a new instance.

```
var p = Point();
p.x = 10;
```

### Method Definition

Methods are defined **outside** the class body using the `method` keyword. This syntax explicitly binds a function to a receiver type.

**Syntax**: `method (receiver_name ClassName) MethodName(args...) { ... }`

```
method (p Point) move(dx, dy){
    p.x = p.x + dx;
    p.y = p.y + dy;
}

var p = Point();
p.move(5, 5);
```

Inside a method, you can use the `this` keyword to refer to the instance (receiver), or use the name you defined in the method signature (`p` in the example above). Both refer to the same object (Local slot 0).

### Constructors

PiCo uses a special method named `init` as the constructor to initialize new instances.

- **Automatic Invocation**: When you instantiate a class (e.g., `var p = Point(10, 20);`), PiCo automatically calls the `init` method matching that class with the provided arguments.
- **Implicit Return**: The `init` method is special; it implicitly returns the new instance (`this`). You do not need to write `return`, and returning a specific value from `init` is a syntax error.

Example:

```
class Point {
    x = 0;
    y = 0;
}

# The constructor method
method (p Point) init(x, y) {
    p.x = x;
    p.y = y;
    # 'this' (p) is returned automatically
}

var p = Point(10, 20); # Calls init implicitly
```

### Visibility

PiCo enforces access control based on **Capitalization**.

- **Public**: Field names starting with an Uppercase letter (e.g., `Name`, `ID`) are accessible from anywhere.

- **Private**: Field names starting with a lowercase letter (e.g., `age`, `hidden`) are only accessible within methods of that class.

Example:

```
class User{
    Name;  # Public
    age;   # Private
}

method (u User) getAge(){
    return u.age; # Accessed inside a method of User
}

var u = User();
u.Name = "Alice";
# u.age = 25;     # Runtime Error: Cannot access private field 'age'
```

## Modules

PiCo uses a file-based module system. Each file is a separate module.

### Importing

Use the `import` keyword to load another `.pcs` file.

- **Syntax**: `import "path/to/mod.pcs";`

- **Behavior**:
  
  1. PiCo executes the referenced file.
  
  2. All global variables defined in that file become properties of the module object.
  
  3. A variable with the same name as the file (without extension) is automatically created in the current scope to reference the module.

Example:

- FIle `math.pcs`:

```
func add(a, b) { return a + b; }
var PI = 3.14;
```

- File `main.pcs`:

```
import "math.pcs"; # Creates variable 'math'

print math.PI;
print math.add(10, 20);
```

### Automatic Module Naming

When importing a module, PiCo automatically creates a variable derived from the filename (without extension and directory path) to reference the module. Custom aliasing (e.g., `as`) is not currently supported.

Example:

```
import "./utils/logger.pcs"; 
# Automatically creates a variable named 'logger'
logger.log("Ready");
```

## Standard Libs

### fs - File System Modules

The `fs` module provides functionality for interacting with the file system.

#### Import:

```
import "fs";
```

#### Functions:

- `fs.read(path)`
  
  - *Description*: Reads the entire content of the file at `path`.
  
  - *Arguments*: `path` (String).
  
  - *Returns*: String (content).

- `fs.write(path, content)`
  
  - *Description*: Writes `content` to the file at `path`. **Overwrites** the file if it exists.
  
  - *Arguments*: `path` (String), `content` (String).
  
  - *Returns*: `true` on success, `null` on failure.

- `fs.append(path, content)`
  
  - *Description*: Appends `content` to the end of the file at `path`.
  
  - *Arguments*: `path` (String), `content` (String).
  
  - *Returns*: `true` on success.

- `fs.exists(path)`
  
  - *Description*: Checks if a file exists at the given path.
  
  - *Returns*: `true` if exists, `false` otherwise.

- `fs.remove(path)`
  
  - *Description*: Deletes the file at `path`.
  
  - *Returns*: `true` on success.

- `fs.list(arg)`
  
  - *Description*: Lists all files and directories in the specified directory. If `arg` is a string, it treats it as a directory path and lists all files/directories in it (excluding `.` and `..`). If `arg` is a `glob.Glob` object, it performs a search based on the object's configuration.
  
  - *Returns*: A List of Strings (filenames).

- `fs.rlines(path)`
  
  - *Description*: Reads the file line by line.
  
  - *Returns*: A List of Strings, where each item is a line from the file.

- `fs.mkdir(path)`
  
  - *Description*: Creates a new directory.
  
  - *Returns*: `true` on success.

- `fs.isDir(path)`
  
  - *Description*: Checks if the path points to a directory.
  
  - *Returns*: `true` if it is a directory.

- `fs.open(path, [mode])`
  
  - *Description*: Opens a file and returns a **File Object** for advanced operations.
  
  - *Arguments*:
    
    - `path` (String).
    
    - `mode` (String, optional): "r" (read), "w" (write), etc. Defaults to "r".
  
  - *Returns*: A File Object.

### os - Operating System Module

The `os` module provides tools to interact with the underlying operating system.

#### Import:

```
import "os";
```

#### Functions:

- `os.run(command)`
  
  - *Description*: Executes a shell command using `system()`.
  
  - *Arguments*: `command` (String).
  
  - *Returns*: The exit status code (number).

- `os.exec(command)`
  
  - *Description*: Executes a shell command and captures its output (stdout).
  
  - *Arguments*: `command` (String).
  
  - *Returns*: String (the output of the command).

- `os.getenv(name)`
  
  - *Description*: Gets the value of an environment variable.
  
  - *Returns*: String (value) or `null` if not found.

- `os.exit(code)`
  
  - *Description*: Terminates the program immediately with the given exit code.
  
  - *Arguments*: `code` (Number).

### time - Time Module

#### Import:

```
import "time";
```

#### Functions:

- `time.now()`
  
  - *Returns*: The current high-resolution wall-clock time in seconds (Double).

- `time.steady()`
  
  - *Returns*: A monotonic clock time in seconds, suitable for measuring intervals.

- `time.clock()`
  
  - *Returns*: System time in seconds (Standard C `time(NULL)`).

- `time.sleep(seconds)`
  
  - *Description*: Pauses execution for the specified number of seconds.
  
  - *Arguments*: `seconds` (Number).

- `time.fmt(timestamp, [format])`
  
  - *Description*: Formats a timestamp into a readable string.
  
  - *Arguments*:
    
    - `timestamp` (Number).
    
    - `format` (String, optional): Default is `"%Y-%m-%d %H:%M:%S"`.
  
  - *Returns*: String.

### path - Path Module

Utilities for handling file paths cross-platform.

#### Import:

```
import "path";
```

#### Functions:

- `path.join(part1, part2, ...)`
  
  - *Description*: Joins multiple path segments using the system separator.

- `path.base(path)`
  
  - *Returns*: The filename portion of the path.

- `path.dirname(path)`
  
  - *Returns*: The directory portion of the path.

- `path.ext(path)`
  
  - *Returns*: The file extension (including `.`).

- `path.abs(path)`
  
  - *Returns*: The absolute path.

- `path.isAbs(path)`
  
  - *Returns*: `true` if the path is absolute.

- `path.sep()`
  
  - *Returns*: The system path separator (`/` or `\`).

### glob - Glob Module

#### Import:

```
import "glob";
```

#### Funtions:

- `glob.match(pattern, text)`
  - *Description*: Checks if the text string matches the specified pattern. It supports:
  - Wildcards: * (any sequence), ? (single char).
  - Character Sets: [a-z], [!0-9] (negation).
  - Numeric Ranges: {0..10} (inclusive).
  - Enumerations: {jpg, png, gif}.
  - Deep Matching: ** (matches across directories).
  - *Arguments*: pattern (String), text (String).
  - *Returns*: true if it matches, false otherwise.

Example:

```
glob.match("*.txt", "doc.txt");            # true
glob.match("img_{0..5}.png", "img_2.png"); # true
```

#### `glob.Glob`

The `Glob` class is used to configure complex file searches. Instances of this class can be passed to `fs.list()` to retrieve matching files.

Properties:
- `Dir` (String): The root directory to start the search from. Defaults to "`.`" (current directory).
- `Pattern` (String): The glob pattern to match files against (e.g., "`*.txt`", "`**/*.c`"). Defaults to "`*`" (match all files).
- `Recursive` (Boolean): If true, the search will traverse subdirectories recursively. Defaults to false.
- `IgnoreCase` (Boolean): If true, the pattern matching and filename comparison will be case-insensitive. Defaults to false.
- `Exclude` (List): A list of glob pattern strings. Files matching any pattern in this list will be excluded from the results. Defaults to null.

Example:

To search for files, instantiate `glob.Glob`, configure its properties, and pass the object to `fs.list()`.

```
import "fs";
import "glob";

var config = glob.Glob();
config.Dir = "src";
config.Pattern = "*.c";

var c_files = fs.list(config);

var search = glob.Glob();
search.Dir = ".";
search.Recursive = true;
search.Pattern = "**/*.{c,h}";        # Match C sources and headers
search.Exclude = ["vendor/*", "tmp"]; # Exclude specific directories
search.IgnoreCase = true;             # Match .C or .H too

var all_source_files = fs.list(search);
```

### string - String Module

#### Import:

```
import "string";
```

#### Functions:

- `string.ascii(code)`
  - *Description*: Returns a one-character string corresponding to the given ASCII/Unicode code point.
  - *Arguments*: `code` (Number).
  - *Returns*: String.

## Built-in Object Methods

Certain built-in types have methods attached to them automatically.

### List Methods

Available on any List object (e.g., `[1, 2]`).

- `.push(item)`: Adds `item` to the end of the list. Returns the new size.

- `.pop()`: Removes and returns the last item from the list.

- `.size()`: Returns the number of elements in the list.

### File Object Methods

Available on file objects returned by `fs.open()`.

- `.read()`: Reads the rest of the file content.

- `.readLine()`: Reads a single line from the file.

- `.write(string)`: Writes a string to the file.

- `.close()`: Closes the file handle.

### String Method

Available on any String object (e.g., `"hello"`).

- `.len()`
  
  - *Description*: Returns the length of the string.
  
  - *Returns*: Number.

- `.sub(start, [end])`
  
  - *Description*: Returns a substring from `start` index up to (but not including) `end`. Supports negative indexing (e.g., `-1` is the last character).
  
  - *Arguments*:
    
    - `start` (Number).
    
    - `end` (Number, optional): Defaults to the end of the string.
  
  - *Returns*: String.

- `.trim()`
  
  - *Description*: Removes whitespace from both the beginning and the end of the string.
  
  - *Returns*: String.

- `.upper()`
  
  - *Description*: Returns a copy of the string converted to uppercase.
  
  - *Returns*: String.

- `.lower()`
  
  - *Description*: Returns a copy of the string converted to lowercase.
  
  - *Returns*: String.

- `.find(substring)`
  
  - *Description*: Searches for the first occurrence of `substring`.
  
  - *Arguments*: `substring` (String).
  
  - *Returns*: Number (the index of the first match, or `-1` if not found).

- `.split(delimiter)`
  
  - *Description*: Splits the string into a list of substrings based on the `delimiter`. If `delimiter` is an empty string `""`, it splits the string into individual characters.
  
  - *Arguments*: `delimiter` (String).
  
  - *Returns*: List of Strings.

- `.replace(old, new)`
  
  - *Description*: Returns a new string with all occurrences of `old` replaced by `new`.
  
  - *Arguments*: `old` (String), `new` (String).
  
  - *Returns*: String.

## REPL

### Launch the REPL

To start the REPL, simply run the PiCo executable without any arguments in your terminal.

```bash
$ ./pico
PiCo REPL. Press Ctrl+C to exit.
>>>
```

### Features

The PiCo REPL is powered by the `linenoise` library, offering modern terminal features:

#### Persistent History

- *Behavior*: The REPL remembers 100 lines of your previously entered commands even after you exit.

- *Storage*: History is saved to a hidden file named `.pico_history` in the PiCo directory.

- *Navigation*: Use the **Up** and **Down** arrow keys to scroll through your command history.

#### Tab Auto-Completion

Pressing the **Tab** key triggers auto-completion for PiCo keywords.

### Exiting

To exit the REPL session, press **Ctrl+C**.
