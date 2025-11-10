# Assignment 3: Lowering AST-->LIR

__RELEASED:__ Tuesday, Nov 4
__DUE:__ Thursday, Nov 13
__LATE DEADLINE 1:__ Thursday, Nov 20 (-11%)
__LATE DEADLINE 2:__ Friday, Dec 12 (-31%)

You may work in groups of 1--3 people. If working in a group of 2--3, be sure to indicate that when submitting to Gradescope.

## Description

Implement the lowering pass from AST to LIR per the `lower.md` document given in the lecture notes. Your implementation will take valid ASTs in JSON format and output the corresponding LIR program.

## Input/Output Specifications

The input will be a file containing a JSON representation of a valid AST. You may use the same `json.cpp` JSON library to parse the input as you used in assign-2.

The output should be the corresponding LIR program, in the same format as given by my `cflat` implementation (as compared using `diff -wB`). Note that the output must be ordered the same way as my implementation (lexicographically by identifier).

## Grading

The grading will be done using a set of test suites, weighted evenly. The test suites for this assignment are:

- `TS1`: `main` function, constants, locals, assignments, return
- `TS2`: TS1 + unary and binary operators
- `TS3`: TS2 + if, while, continue, break, select
- `TS4`: TS3 + non-{struct, function} pointers, nil, singleton allocation
- `TS5`: TS3 + arrays, array accesses, nil, array allocation
- `TS6`: TS3 + structs, struct allocation, struct accesses
- `TS7`: TS3 + other functions, externs, function pointers, direct calls, indirect calls
- `TS8`: everything in TS{1--7} all together

## Reference Solution

I have placed an executable of my own Cflat compiler implementation on CSIL in `~benh/160/cflat`. Use `cflat --help` to see how to use it. In particular for this assignment:

- You can use `cflat` to translate a `*.cb` or `*.astj` file into the `*.lir` format using `cflat -o lir <file>.{cb, astj}`.

- As before you can recover the source code from a `*.astj` file using `cflat -o source <file>.astj` to help you see what the program is doing.

You can use the reference solution to test your code before submitting. If you have any questions about the output format or the behavior of the implementation you can answer them using the reference solution; this is the solution that Gradescope will use to test your submissions.

## Submitting to Gradescope

The autograder is running Ubuntu 22.04 and has the latest `build-essential` package installed (including `g++` version 11.4.0 and the `make` utility). You may not use any other libraries or packages for your code. Your submission must meet the following requirements:

- There must be a `makefile` file s.t. invoking the `make` command builds your code, resulting in an executable called `lower`. The executable must take a single argument (the file to run the lowerer on) and produce its output on standard out.

- If your solution contains sub-directories then the entire submission must be given as a zip file (this is required by Gradescope for submissions that contain a directory structure, otherwise it will automatically flatten everything into a single directory). The `makefile` should be in the root directory of the solution.

- The submitted files are not allowed to contain any binaries, and your solution is not allowed to use the network.

## Use AddressSanitizer

Student programs can often contain memory errors that happen not to manifest themselves when the students execute their code locally, but do manifest themselves when the program is uploaded and executed by Gradescope. This difference in behavior can result in confusion because Gradescope is claiming there is an error, but there is seemingly no error when the students run the program themselves (note that there actually is an error, it just happens to not be caught at runtime). To avoid this issue, please always compile your code using AddressSanitizer, which will catch most such errors locally. See the following site for an explanation of how to do so (the given flags are for the `clang` compiler, but they should be the same for `gcc`): https://github.com/google/sanitizers/wiki/AddressSanitizer.
