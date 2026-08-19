<p align="center">
  <img src="assets/logo.svg" alt="Virl" width="420">
</p>

<p align="center">
  <b>Python-like simplicity with C/C++-like structure.</b><br>
  A small, general-purpose programming language with a real tree-walking interpreter.
</p>

```virl
fn main() {
    let name = "Virl";
    let age = 18;

    if age >= 18 {
        print("Hello " + name);
    }
}
```

```
$ ./bin/virl examples/hello.virl
Hello Virl
```

## Quick start

```bash
make
./bin/virl examples/hello.virl
./bin/virl --list-builtins       # see every built-in function
./bin/virl --help
```

## Project structure

The codebase is organized by pipeline stage, so extending any one part
never requires touching the others:

```
virl/
├── assets/
│   ├── logo.svg           full wordmark (used above)
│   ├── icon.svg            square icon, source of the PNG/ICO below
│   ├── icon.png / icon-512.png
│   └── icon.ico             Windows multi-resolution icon
│
├── src/
│   ├── lexer/            source text -> tokens
│   │   ├── token.hpp/.cpp
│   │   └── lexer.hpp/.cpp
│   │
│   ├── parser/            tokens -> AST
│   │   ├── ast.hpp          every expression & statement node
│   │   └── parser.hpp/.cpp  recursive-descent parser
│   │
│   ├── runtime/            AST -> execution
│   │   ├── errors.hpp        shared RuntimeError type
│   │   ├── value.hpp/.cpp    the Value type + Environment (scopes)
│   │   └── interpreter.hpp/.cpp   tree-walking evaluator
│   │
│   ├── builtins/          the standard library
│   │   ├── builtins.hpp/.cpp      registry + shared arg-validation helpers
│   │   ├── builtins_core.cpp      print, len, typeof, toString, toNumber, input, range
│   │   ├── builtins_math.cpp      abs, sqrt, pow, min, max, floor, ceil, round, random, randomInt
│   │   ├── builtins_string.cpp    upper, lower, trim, split, join, contains, replace, substring
│   │   └── builtins_array.cpp     push, pop, first, last, reverse, sort, slice, indexOf
│   │
│   └── cli/                the `virl` executable
│       ├── colors.hpp        ANSI color helpers (TTY-aware)
│       └── main.cpp
│
├── examples/
│   ├── hello.virl
│   ├── showcase.virl
│   ├── fib.virl
│   └── builtins_demo.virl   tour of the whole standard library
│
├── tests/                 (reserved for future automated tests)
├── Makefile
└── README.md
```

### Adding a new builtin function

This is the main thing the folder structure was designed around:

1. Open the right category file under `src/builtins/` (or add a new `builtins_yourcategory.cpp`).
2. Add one `reg.add("name", [](std::vector<Value>& args, int line, int column) -> Value { ... });` call.
3. Use the `argcheck::` helpers (`exactCount`, `rangeCount`, `number`, `string`, `array`) for validation, so errors look and read like every other builtin's.
4. If it's a new file, add `void registerYourCategoryBuiltins(BuiltinRegistry&);` to `builtins.hpp` and call it from `BuiltinRegistry`'s constructor in `builtins.cpp`.

The Makefile discovers `.cpp` files automatically (`find src -name '*.cpp'`), so no build-file edits are needed either way.

## Building

```bash
make              # incremental build (only rebuilds changed files)
make clean        # remove bin/
make run          # build + run examples/hello.virl
```

Or by hand:

```bash
# Linux / macOS
g++ -std=c++17 -Wall -Wextra -O2 -Isrc -o bin/virl $(find src -name '*.cpp')

# Windows (MinGW)
g++ -std=c++17 -Wall -Wextra -O2 -Isrc -o bin/virl.exe $(find src -name '*.cpp')
```

## Running

```bash
./bin/virl program.virl              # run a program
./bin/virl --tokens program.virl     # debug: print the colorized token stream
./bin/virl --list-builtins           # list every built-in function
./bin/virl --version                 # print the version banner
./bin/virl --help                    # usage
```

Add `--no-color` to any command to disable ANSI colors (also disabled automatically when output isn't a terminal, e.g. `./bin/virl foo.virl > log.txt`).

## Visual identity

`assets/` has a wordmark (`logo.svg`) and a square icon in SVG, PNG (256×256, 512×512), and Windows `.ico` form. To actually use `icon.ico`/`icon.png` as the icon `.virl` files show in your file explorer, you associate it through your OS the normal way a file type gets an icon — there's no way to do that from inside this project itself:

- **Windows:** create/edit a registry entry for the `.virl` extension pointing its icon at `assets/icon.ico` (Default Programs settings, or `HKEY_CLASSES_ROOT\.virl\DefaultIcon`).
- **macOS:** select a `.virl` file → `Get Info` → drag `icon.png` onto the small icon in the top-left of the Info panel. To apply it to all `.virl` files, do this once on a file, then use a utility like `fileicon` (or Automator) to apply it system-wide.
- **Linux (GNOME/KDE):** add a MIME type for `.virl` (an `.xml` entry under `~/.local/share/mime/packages/`) and point its `<icon name="...">` at `icon.png`, then update the icon cache.

The CLI itself also shows a small colored banner (`./bin/virl --version`) so the language has a consistent identity even without OS-level icon registration.

## Language reference

**Keywords:** `let const fn return if else while for in true false null break continue`

**Operators:** `+ - * / % = == != > < >= <= && || !`

**Symbols:** `{ } ( ) [ ] ; , . :`

**Literals:** numbers (unified int/float — see below), strings with `\n \t \" \\` escapes, `true`/`false`, `null`, arrays `[1, 2, 3]`.

**Statements:**
```virl
let x = 10;                    // mutable
const pi = 3.14159;            // immutable — reassigning is a runtime error
let y: int = 5;                // optional type annotation (parsed, not enforced yet)

if x > 5 { ... } else if x > 0 { ... } else { ... }
while x > 0 { x = x - 1; }
for i in range(0, 10) { ... }  // range(end) / range(start,end) / range(start,end,step)
for item in someArray { ... }  // for-in also works directly over arrays

fn add(a, b) { return a + b; }
fn greet(name: string) -> string { return "Hi " + name; }  // types parsed, not checked yet

break;
continue;
```

**Entry point:** if the program defines a function named `main`, the interpreter calls it automatically after all top-level declarations run.

## Standard library (33 builtins)

| Category | Functions |
|---|---|
| **Core** | `print`, `len`, `typeof`, `toString`, `toNumber`, `input`, `range` |
| **Math** | `abs`, `sqrt`, `pow`, `min`, `max`, `floor`, `ceil`, `round`, `random`, `randomInt` |
| **String** | `upper`, `lower`, `trim`, `split`, `join`, `contains`, `replace`, `substring` |
| **Array** | `push`, `pop`, `first`, `last`, `reverse`, `sort`, `slice`, `indexOf` |

Run `./bin/virl --list-builtins` for the live list, or see `examples/builtins_demo.virl` for every function in action.

A couple of behaviors worth knowing:
- `push`, `pop`, `reverse`, `sort` **mutate their array argument in place** (and `reverse`/`sort` also return it, so calls can be chained). `slice` returns a **new** array instead.
- `min`/`max` are variadic: `min(1, 2, 3)` and `min([1, 2, 3])` both work.
- A program can shadow a builtin name (e.g. define its own `len`) — user-defined names are always checked before falling back to the builtin registry.

## Design decisions

- **Numbers are unified.** The lexer still distinguishes `Integer`/`Float` tokens, but at runtime both become a single `double` — closer to JavaScript's one `number` type than C++'s int/float split.
- **`&&`/`||` coerce to boolean**, rather than passing through whichever operand "won" — chosen for predictability.
- **Functions are constant bindings** (`fn greet(){}` behaves like `const greet = ...`), matching how most scripting languages protect top-level declarations.
- **Only arrays are indexable** with `arr[i]` right now, not strings — use `substring()` for that.
- **No block comments yet** — only `// line comments`.

## Current limitations

- No hexadecimal/binary/scientific-notation number literals.
- No block comments (`/* ... */`).
- Strings cannot span multiple lines.
- Type annotations (`: int`, `-> string`) are parsed but not enforced — full dynamic typing for now.
- No modules/imports, no structs/classes, no bytecode VM (this is a direct AST-walking interpreter).
- No try/catch — a runtime error stops the whole program.

## Roadmap

- **Done:** lexer, grammar, parser, AST, tree-walking interpreter (variables, constants, functions with recursion & closures, if/else, while, for-in, arrays, all Phase-1 operators, break/continue/return), a 33-function standard library, and a colorized CLI.
- **Next up:** a real type system, modules/imports, structured error handling (try/catch), and — longer term — deciding whether Virl moves to a bytecode VM for performance.
