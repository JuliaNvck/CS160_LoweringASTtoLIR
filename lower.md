# AST ⟶ LIR transformation
## Overview

We need to translate from the AST data structure to the LIR data structure. Note that:

- LIR still uses `Type` just like AST.

- LIR has `Program` and `Function` like AST, but with different field types (mainly because now that we've ensured there are no duplicate names, we can safely use maps instead of sets).

- LIR `Program` has an additional field `funptrs` that maps each function's name to a function pointer `Type` (except for `main`, which can't be called). For example, a function declared as `fn foo(p:int, q:int) -> int` would be inserted into `funptrs` as `"foo" ↦ Ptr(Fn([int,int], int))`.

- LIR has some new data types, specifically for a function's body, that we use to represent a function's control-flow graph (CFG).

Converting everything but function bodies from AST to LIR is trivial, the only complicated part is lowering AST statements and expressions into LIR instructions and organizing them into a LIR control-flow graph. Our strategy for translating a function's body is:

1. Lower the statements into a single _translation vector_ whose elements can be either LIR instructions or labels `Label(name)`, where `name` is a string representing a basic block label.

2. Take the resulting vector and use it to construct the control-flow graph for the corresponding LIR function body.

The remainder of this document explains lowering from AST to LIR in detail.

## Helper Functions

- `fresh_inner_var(τ)`: Returns a fresh variable (i.e., unique within this function) of type τ. Reuses a previously-created fresh variable if possible, that is, if one of type τ was created already for this function and then released (see `release` below); otherwise creates a new fresh variable and inserts it onto the enclosing function's locals.

    - The name should be `_inner<num>`, where `<num>` is a counter that is incremented each time `fresh_{inner, non_inner}_var` are called.

    - This helper should be used to create temporary variables that hold pointers into the internals of structs or arrays, i.e., they are being assigned the result of `$gfp` or `$gep`.

- `fresh_non_inner_var(τ)`: Returns a fresh variable (i.e., unique within this function) of type τ. Reuses a previously-created fresh variable if possible, that is, if one of type τ was created already for this function and then released (see `release` below); otherwise creates a new fresh variable and inserts it onto the enclosing function's locals.

    - The name should be `_tmp<num>`, where `<num>` is a counter that is incremented each time `fresh_{inner, non_inner}_var` are called.

    - This helper should be used to create all other temporary variables.

- `release([op...])`: Releases previously-created fresh variables to be used again by `fresh_inner_var()` and `fresh_non_inner_var()`. For convenience the arguments can include user-defined program variables as well as variables created by `fresh_inner_var()` and `fresh_non_inner_var()`---only the fresh temporaries are considered released, the user-defined variables are ignored.

- `const(n)`: Returns a variable whose value is the constant `n`. If one doesn't currently exist then (1) creates the variable and inserts it into the function's local variables, and (2) inserts a `$const` instruction at the top of the `entry` block assigning its value.

    - The variables holding these constant values should be named `_const_<num>`, where `<num>` is the constant value (negative values should have an `n` in front instead of a `-`). For example, the variable holding the constant `-12` should be named `_const_n12`.

- `label()`: Creates and returns a fresh basic block label. The label should be named `lbl<num>`, where `<num>` is a counter that is incremented each time `label` is called.

- `typeof(x)`: Returns the type of variable `x`. The type is retrieved by looking first at the enclosing function's locals, then it's parameters, then `LIR::Program.{funptrs, externs}`.

## Lowering AST⟶LIR
### Lowering AST::Program `prog`

- Create an empty `LIR::Program` named `lir`.

- Copy `prog.{externs, structs, functions}` into `lir` (translating from sets to maps) _except_ leaving all function bodies empty.

- ∀`f` ∈ `prog.functions` \ {`main`}: `lir.funptrs` += [`f.name` ⟶ `Ptr(Fn(f.params.types, f.rettyp))`].

- ∀`f` ∈ `prog.functions`: lower `f` per the explanation below.

### Lowering AST::Function `fun`

- Create the translation vector `tv`, initialized to `[Label("{fun.name}_entry")]`

    - Here and in the remaining sections, text after `%` are LIR instructions and labels to be emitted by the lowerer into `tv`.

- Compute `⟦f.stmts⟧ˢ`, filling in `tv`.

- Construct the CFG for `lir.functions[f.name].body` from `tv`.

### Lowering Statements

- Signature: `⟦Stmt⟧ˢ ⟶ ()`

```
⟦stmts⟧ˢ = ∀s ∈ stmts.⟦s⟧ˢ

⟦Assign(lhs, rhs)⟧ˢ =
  if lhs is Id(name) then
    let x = ⟦rhs⟧ᵉ
    % Copy(Var(name), x)
    release([x])
  else
    let x = ⟦lhs⟧ˡ
    let y = ⟦rhs⟧ᵉ
    % Store(x, y)
    release([x, y])

⟦FunCall(callee, args)⟧ˢ =
  let xs = ∀a ∈ args.⟦a⟧ᵉ (in reverse order)
  let fun = ⟦callee⟧ᵉ
  % Call(None, fun, xs)
  release(xs ++ [fun])

⟦If(guard, tt, ff)⟧ˢ =
  let TT = label(), FF = label(), END = label()
  let x = ⟦guard⟧ᵉ
  % Branch(x, TT, FF)
  % Label(TT)
  release([x])
  ⟦tt⟧ˢ
  % Jump(END)
  % Label(FF)
  ⟦ff⟧ˢ
  % Jump(END)
  % Label(END)

⟦While(guard, body)⟧ˢ =
  let LOOP_HDR = label(), BODY = label(), LOOP_END = label()
  % Jump(LOOP_HDR)
  % Label(LOOP_HDR)
  let x = ⟦guard⟧ᵉ
  % Branch(x, BODY, LOOP_END)
  release([x])
  % Label(BODY)
  ⟦body⟧ˢ
  % Jump(LOOP_HDR)
  % Label(LOOP_END)

⟦Break⟧ˢ =
  find nearest previous Branch(_,_,LOOP_END)
  % Jump(LOOP_END)

⟦Continue⟧ˢ =
  find nearest previous Label(LOOP_HDR)
  % Jump(LOOP_HDR)

⟦Return(e)⟧ˢ =
  let x = ⟦e⟧ᵉ
  % Return(x)
  release([x])
```

### Lowering Expressions

- Signature: `⟦Exp⟧ᵉ ⟶ VarId`

- Note that we translate `nil` into a variable named `__NULL`; currently that variable doesn't exist, but we'll create it as a global when we do code generation. We use a variable instead of a constant value (like `0`) because different machines can have different values for a null pointer.

- The `select` translation must handle the case when one or both branch expressions result in `nil` specially, because we shouldn't return a fresh variable whose type is `nil` (if we did and one of the branches wasn't `nil` we would get a type error; even if both branches are `nil` this select could be used inside another select, one of whose branches is not `nil`, and we would again get a type error). It handles this by relying on the facts: (1) `__NULL` is the lowered value for `nil`; and (2) the default value for any declared pointer is `__NULL`.

```
⟦Val(Id(name))⟧ᵉ = Var(name)

⟦Val(place ̸= Id(name))⟧ᵉ =
  let src = ⟦place⟧ˡ
  let lhs = fresh_non_inner_var(Ptr(typeof(src)))
  % Load(lhs, src)
  release([src])
  lhs

⟦Num(n)⟧ᵉ = const(n)

⟦Nil⟧ᵉ = Id("__NULL")

⟦Select(g, tt, ff)⟧ᵉ =
  let TT = label(), FF = label(), END = label()
  let x = Id("__NULL")
  let y = ⟦g⟧ᵉ
  % Branch(y, TT, FF)
  % Label(TT)
  release([y])
  let z = ⟦tt⟧ᵉ
  if z != Id("__NULL"):
    x = fresh_non_inner_var(typeof(z))
    % Copy(x, z)
  release([z])
  % Jump(END)  
  % Label(FF)
  let w = ⟦ff⟧ᵉ
  if w != Id("__NULL"):
    if x == Id("__NULL"):
      x = fresh_non_inner_var(typeof(w))
    % Copy(x, w)
  release([w])
  % Jump(END)
  % Label(END)
  x

⟦UnOp(Neg, arg)⟧ᵉ =
  if arg is Num(n) then const(-n)
  else
    let lhs = fresh_non_inner_var(Int)
    let zero = const(0)
    let x = ⟦arg⟧ᵉ
    % Arith(lhs, Sub, zero, x)
    release([x])
    lhs

⟦UnOp(Not, arg)⟧ᵉ = ⟦BinOp(Eq, arg, Num(0))⟧

⟦BinOp(op ∈ {Add,Sub,Mul,Div}, left, right)⟧ᵉ =
  let op1 = ⟦left⟧ᵉ
  let op2 = ⟦right⟧ᵉ
  let lhs = fresh_non_inner_var(Int)
  % Arith(lhs, op, op1, op2)
  release([op1, op2])
  lhs

⟦BinOp(op ∈ {Eq, NotEq, Lt, Lte, Gt, Gte}, left, right)⟧ᵉ =
  let op1 = ⟦left⟧ᵉ
  let op2 = ⟦right⟧ᵉ
  let lhs = fresh_non_inner_var(Int)
  % Cmp(lhs, op, op1, op2)
  release([op1, op2])
  lhs

⟦BinOp(And, left, right)⟧ᵉ = ⟦Select(left, right, Num(0))⟧ᵉ

⟦BinOp(Or, left, right)⟧ᵉ =
  let FF = label(), END = label()
  let x = ⟦left⟧ᵉ
  let y = fresh_non_inner_var(Int)
  % Copy(y, x)
  % Branch(y, END, FF)
  % Label(FF)
  release([x])
  let z = ⟦ff⟧ᵉ
  % Copy(y, z)
  release([z])
  % Jump(END)
  % Label(END)
  y

⟦NewSingle(τ)⟧ᵉ
  let lhs = fresh_non_inner_var(Ptr(τ))
  % AllocSingle(lhs, τ)
  lhs

⟦NewArray(τ, amt)⟧ᵉ =
  let lhs = fresh_non_inner_var(Array(τ))
  let x = ⟦amt⟧ᵉ
  % AllocArray(lhs, x, τ)
  release([x])
  lhs

⟦FunCall(callee, args)⟧ᵉ =
  let xs = ∀a ∈ args.⟦a⟧ᵉ (in reverse order)
  let fun = ⟦callee⟧ᵉ
  let lhs = fresh_non_inner_var(τ) s.t. typeof(fun) ∈ {Fn(_,τ), Ptr(Fn(_,τ))}
  % Call(lhs, fun, zs)
  release(xs ++ [fun])
  lhs
```

### Lowering Places

- Signature: `⟦Place⟧ˡ ⟶ VarId`

```
⟦Deref(e)⟧ˡ = ⟦e⟧ᵉ

⟦ArrayAccess(arr, index)⟧ˡ =
  let src = ⟦arr⟧ᵉ
  let idx = ⟦index⟧ᵉ
  let lhs = fresh_inner_var(Ptr(τ)) s.t. typeof(src) = Array(τ)
  % Gep(lhs, src, idx, true)
  release([src, idx])
  lhs

⟦FieldAccess(ptr, fld)⟧ˡ =
  let src = ⟦ptr⟧ᵉ
  let sid = id s.t. typeof(src) = Ptr(Struct(id))
  let lhs = fresh_inner_var(Ptr(typeof(sid[fld])))
  % Gfp(lhs, src, sid, fld)
  release([src])
  lhs
```

### Constructing LIR::Function.body `body` from translation vector `tv`

- Identify the _leader_ instructions, i.e., all LIR instructions that immediately follow a `Label` in `tv`.

- For each leader, which must follow some `Label(name)` in `tv`:

    - Create a LIR::BasicBlock `bb` with label `name`.

    - Copy the leader and all following instructions from `tv` into `bb.insts` until we reach an LIR terminal instruction (`$jmp`, `$branch`, or `$return`), which may be the leader itself.

    - Insert `bb` into `body`.

- Remove any basic blocks in `body` that are unreachable from the basic block labeled `entry`.
