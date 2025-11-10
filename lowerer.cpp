#include "lowerer.hpp"
#include <stdexcept>
#include <iostream>

// --- Main Entry Point ---

std::unique_ptr<LIR::Program> Lowerer::lower(AST::Program* ast_prog) {
    m_lir_prog = std::make_unique<LIR::Program>();
    
    // This will trigger visit(AST::Program* n)
    ast_prog->accept(*this);
    
    return std::move(m_lir_prog);
}

// --- Visitor Implementations: Top-Level ---

void Lowerer::visit(AST::Program* n) {
    // 1. Copy structs
    for (const auto& ast_struct : n->structs) {
        ast_struct->accept(*this);
    }
    
    // 2. Copy externs (process directly, not via visitor)
    for (const auto& ast_extern : n->externs) {
        std::vector<LIR::TypePtr> param_types;
        for (const auto& p_type : ast_extern.param_types) {
            param_types.push_back(convert_type(p_type));
        }
        auto ret_type = convert_type(ast_extern.rettype);
        m_lir_prog->externs[ast_extern.name] = std::make_shared<LIR::FnType>(param_types, ret_type);
    }

    // 3. Create function shells and populate funptrs (for internal functions)
    // ∀`f` ∈ `prog.functions` \ {`main`}: `lir.funptrs` += [`f.name` ⟶ `Ptr(Fn(f.params.types, f.rettyp))`].
    for (const auto& ast_fun : n->functions) {
        LIR::Function lir_fun;
        lir_fun.name = ast_fun->name;
        lir_fun.rettyp = convert_type(ast_fun->rettype);

        std::vector<LIR::TypePtr> param_types;
        for (const auto& p : ast_fun->params) {
            LIR::TypePtr param_type = convert_type(p.type);
            lir_fun.params.push_back({p.name, param_type});
            param_types.push_back(param_type);
            lir_fun.locals[p.name] = param_type; // Add params to locals map
        }
        
        for (const auto& l : ast_fun->locals) {
            lir_fun.locals[l.name] = convert_type(l.type);
        }

        // Add to funptrs (except main)
        if (lir_fun.name != "main") {
            auto fn_type = std::make_shared<LIR::FnType>(param_types, lir_fun.rettyp);
            m_lir_prog->funptrs[lir_fun.name] = std::make_shared<LIR::PtrType>(fn_type);
        }

        m_lir_prog->functions[lir_fun.name] = std::move(lir_fun);
    }

    // 4. Lower each function's body
    for (const auto& ast_fun : n->functions) {
        ast_fun->accept(*this);
    }
}

void Lowerer::visit(AST::StructDef* n) {
    LIR::Struct lir_s;
    lir_s.name = n->name;
    for(const auto& f : n->fields) {
        lir_s.fields[f.name] = convert_type(f.type);
    }
    m_lir_prog->structs[lir_s.name] = std::move(lir_s);
}

void Lowerer::visit(AST::Extern* n) {
    // This visitor is not used since Extern is not a Node.
    // Externs are processed directly in visit(AST::Program*)
    (void)n;
}


void Lowerer::visit(AST::FunctionDef* n) {
    // 1. Set context
    m_current_fun = &m_lir_prog->functions.at(n->name);
    m_tv.clear();
    m_label_counter = 0;
    m_tmp_counter = 0;
    m_const_insert_pos = 1; // Reset const insertion position (after entry label)
    // m_current_fun->locals is already populated with params/locals

    // 2. Create entry label: [Label("{fun.name}_entry")]
    m_tv.push_back(TvLabel{n->name + "_entry"});

    // 3. Compute ⟦f.stmts⟧ˢ
    lower_stmt(n->body.get());

    // 4. Add implicit return if necessary
    // Check if the last terminal in m_tv is a Ret; if not, add one
    bool has_ret = false;
    for (auto it = m_tv.rbegin(); it != m_tv.rend(); ++it) {
        // Check if this item is a Terminal variant
        if (auto* term_ptr = std::get_if<LIR::Terminal>(&(*it))) {
            // Check if the Terminal is a Ret
            if (std::holds_alternative<LIR::Ret>(*term_ptr)) {
                has_ret = true;
                break;
            }
            // If it's another terminal type (Jump/Branch), stop searching
            break;
        }
        // Skip labels when searching backwards
        if (std::holds_alternative<TvLabel>(*it)) {
            continue;
        }
        // If we hit an instruction, stop searching
        if (std::holds_alternative<LIR::Inst>(*it)) {
            break;
        }
    }
    
    if (!has_ret) {
        // Add implicit return (void return)
        m_tv.push_back(LIR::Ret{std::nullopt});
    }

    // 5. Construct CFG
    build_cfg();
}

void Lowerer::visit(AST::Decl* n) {
    // Nothing to do for declarations, they are handled in FunctionDef
    (void)n; 
}

// --- Visitor Implementations: Stmt ---

void Lowerer::visit(AST::Stmts* n) {
    for (const auto& stmt : n->statements) {
        lower_stmt(stmt.get());
    }
}

void Lowerer::visit(AST::Assign* n) {
    // ⟦Assign(lhs, rhs)⟧ˢ =
    
    // Check if lhs is Id(name)
    if (auto id_place = dynamic_cast<AST::Id*>(n->place.get())) {
        // if lhs is Id(name) then
        //   let x = ⟦rhs⟧ᵉ
        LIR::VarId x = lower_exp(n->exp.get());
        //   % Copy(Var(name), x)
        m_tv.push_back(LIR::Copy{id_place->name, x});
        //   release([x])
        release({x});
    } else {
        // else
        //   let x = ⟦lhs⟧ˡ
        LIR::VarId x = lower_place(n->place.get());
        //   let y = ⟦rhs⟧ᵉ
        LIR::VarId y = lower_exp(n->exp.get());
        //   % Store(x, y)
        m_tv.push_back(LIR::Store{x, y});
        //   release([x, y])
        release({x, y});
    }
}

void Lowerer::visit(AST::CallStmt* n) {
    // ⟦FunCall(callee, args)⟧ˢ =
    //   let xs = ∀a ∈ args.⟦a⟧ᵉ (in reverse order)
    std::vector<LIR::VarId> xs;
    for (auto it = n->fun_call->args.rbegin(); it != n->fun_call->args.rend(); ++it) {
        xs.push_back(lower_exp(it->get()));
    }
    
    //   let fun = ⟦callee⟧ᵉ
    LIR::VarId fun = lower_exp(n->fun_call->callee.get());
    
    //   % Call(None, fun, xs)
    m_tv.push_back(LIR::Call{std::nullopt, fun, xs});
    
    //   release(xs ++ [fun])
    xs.push_back(fun);
    release(xs);
}

// --- EXAMPLE: If Stmt ---
void Lowerer::visit(AST::If* n) {
    // ⟦If(guard, tt, ff)⟧ˢ =
    //   let TT = label(), FF = label(), END = label()
    LIR::BbId TT = new_label("if_true");
    LIR::BbId FF = new_label("if_false");
    LIR::BbId END = new_label("if_end");
    
    //   let x = ⟦guard⟧ᵉ
    LIR::VarId x = lower_exp(n->guard.get());
    
    //   % Branch(x, TT, FF)
    m_tv.push_back(LIR::Branch{x, TT, FF});
    
    //   % Label(TT)
    m_tv.push_back(TvLabel{TT});
    
    //   release([x])
    release({x});
    
    //   ⟦tt⟧ˢ
    lower_stmt(n->tt.get());
    
    //   % Jump(END)
    m_tv.push_back(LIR::Jump{END});
    
    //   % Label(FF)
    m_tv.push_back(TvLabel{FF});
    
    //   ⟦ff⟧ˢ
    if (n->ff) {
        lower_stmt(n->ff->get());
    }
    
    //   % Jump(END)
    m_tv.push_back(LIR::Jump{END});
    
    //   % Label(END)
    m_tv.push_back(TvLabel{END});
}

void Lowerer::visit(AST::While* n) {
    // ⟦While(guard, body)⟧ˢ =
    //   let LOOP_HDR = label(), BODY = label(), LOOP_END = label()
    LIR::BbId LOOP_HDR = new_label("loop_hdr");
    LIR::BbId BODY = new_label("loop_body");
    LIR::BbId LOOP_END = new_label("loop_end");
    
    // Push loop labels onto stacks for break/continue
    m_loop_hdr_stack.push_back(LOOP_HDR);
    m_loop_end_stack.push_back(LOOP_END);
    
    //   % Jump(LOOP_HDR)
    m_tv.push_back(LIR::Jump{LOOP_HDR});
    //   % Label(LOOP_HDR)
    m_tv.push_back(TvLabel{LOOP_HDR});
    //   let x = ⟦guard⟧ᵉ
    LIR::VarId x = lower_exp(n->guard.get());
    //   % Branch(x, BODY, LOOP_END)
    m_tv.push_back(LIR::Branch{x, BODY, LOOP_END});
    //   release([x])
    release({x});
    //   % Label(BODY)
    m_tv.push_back(TvLabel{BODY});
    //   ⟦body⟧ˢ
    lower_stmt(n->body.get());
    //   % Jump(LOOP_HDR)
    m_tv.push_back(LIR::Jump{LOOP_HDR});
    //   % Label(LOOP_END)
    m_tv.push_back(TvLabel{LOOP_END});
    
    // Pop loop labels from stacks
    m_loop_hdr_stack.pop_back();
    m_loop_end_stack.pop_back();
}

void Lowerer::visit(AST::Break* n) {
    // ⟦Break⟧ˢ =
    //   find nearest previous Branch(_,_,LOOP_END)
    //   % Jump(LOOP_END)
    (void)n; // Unused parameter
    if (m_loop_end_stack.empty()) {
        throw std::runtime_error("Break statement outside of loop");
    }
    LIR::BbId LOOP_END = m_loop_end_stack.back();
    m_tv.push_back(LIR::Jump{LOOP_END});
}

void Lowerer::visit(AST::Continue* n) {
    // ⟦Continue⟧ˢ =
    //   find nearest previous Label(LOOP_HDR)
    //   % Jump(LOOP_HDR)
    (void)n; // Unused parameter
    if (m_loop_hdr_stack.empty()) {
        throw std::runtime_error("Continue statement outside of loop");
    }
    LIR::BbId LOOP_HDR = m_loop_hdr_stack.back();
    m_tv.push_back(LIR::Jump{LOOP_HDR});
}

void Lowerer::visit(AST::Return* n) {
    //     ⟦Return(e)⟧ˢ =
    //   let x = ⟦e⟧ᵉ
    //   % Return(x)
    //   release([x])
   if (n->exp.has_value()) {
        LIR::VarId x = lower_exp(n->exp.value().get());
        m_tv.push_back(LIR::Ret{x});
        release({x});
    } else {
        m_tv.push_back(LIR::Ret{std::nullopt});
    }
}

// --- Visitor Implementations: Exp ---

void Lowerer::visit(AST::Val* n) {
    // ⟦Val(place)⟧ᵉ =
    // Note: lower.md splits this rule, but my AST wraps all Places in Val
    
    // if place is Id(name) : ⟦Val(Id(name))⟧ᵉ = Var(name)
    if (auto id_place = dynamic_cast<AST::Id*>(n->place.get())) {
        // ⟦Val(Id(name))⟧ᵉ = Var(name)
        m_last_result_id = id_place->name;
    } else {
        // ⟦Val(place ̸= Id(name))⟧ᵉ =
        //   let src = ⟦place⟧ˡ
        LIR::VarId src = lower_place(n->place.get());
        
        //   let lhs = fresh_non_inner_var(Ptr(typeof(src)))
        //   However, src is already Ptr(τ), so we need to extract τ for the Load result
        //   The Load instruction loads a value of type τ from a pointer Ptr(τ)
        LIR::TypePtr ptr_type = typeof_var(src); // Get Ptr(τ)
        LIR::TypePtr val_type = typeof_ptr_element(ptr_type); // Extract τ

        LIR::VarId lhs = fresh_non_inner_var(val_type);
        //   % Load(lhs, src)
        m_tv.push_back(LIR::Load{lhs, src});
        //   release([src])
        release({src});
        //   lhs
        m_last_result_id = lhs;
    }
}

void Lowerer::visit(AST::Num* n) {
    // ⟦Num(n)⟧ᵉ = const(n)
    m_last_result_id = const_var(static_cast<int>(n->value));
}

void Lowerer::visit(AST::NilExp* n) {
    // ⟦Nil⟧ᵉ = Id("__NULL")
    (void)n; // Unused
    m_last_result_id = "__NULL";
}

void Lowerer::visit(AST::Select* n) {
    // TODO: Implement this based on ⟦Select(g, tt, ff)⟧ᵉ
    // This one is tricky! Follow the spec carefully about checking
    // if z == "__NULL" and w == "__NULL".
    //     ⟦Select(g, tt, ff)⟧ᵉ =
    //   let TT = label(), FF = label(), END = label()
    LIR::BbId TT = new_label("if_true");
    LIR::BbId FF = new_label("if_false");
    LIR::BbId END = new_label("if_end");
    //   let x = Id("__NULL")
    LIR::VarId x = "__NULL";
    //   let y = ⟦g⟧ᵉ
    LIR::VarId y = lower_exp(n->guard.get());
    //   % Branch(y, TT, FF)
    m_tv.push_back(LIR::Branch{y, TT, FF});
    //   % Label(TT)
    m_tv.push_back(TvLabel{TT});
    //   release([y])
    release({y});
    //   let z = ⟦tt⟧ᵉ
    LIR::VarId z = lower_exp(n->tt.get());
    //   if z != Id("__NULL"):
    //     x = fresh_non_inner_var(typeof(z))
    //     % Copy(x, z)
    if (z != "__NULL") {
        LIR::TypePtr z_type = typeof_var(z);
        x = fresh_non_inner_var(z_type);
        m_tv.push_back(LIR::Copy{x, z});
    }
    //   release([z])
    release({z});
    //   % Jump(END)  
    m_tv.push_back(LIR::Jump{END});
    //   % Label(FF)
    m_tv.push_back(TvLabel{FF});
    //   let w = ⟦ff⟧ᵉ
    LIR::VarId w = lower_exp(n->ff.get());
    //   if w != Id("__NULL"):
    //     if x == Id("__NULL"):
    //       x = fresh_non_inner_var(typeof(w))
    //     % Copy(x, w)
    if (w != "__NULL") {
        if (x == "__NULL") {
            LIR::TypePtr w_type = typeof_var(w);
            x = fresh_non_inner_var(w_type);
        }
        m_tv.push_back(LIR::Copy{x, w});
    }
    //   release([w])
    release({w});
    //   % Jump(END)
    m_tv.push_back(LIR::Jump{END});
    //   % Label(END)
    m_tv.push_back(TvLabel{END});
    //   x
    m_last_result_id = x;
}

void Lowerer::visit(AST::UnOp* n) {
    switch(n->op) {
        //     ⟦UnOp(Neg, arg)⟧ᵉ =
        case AST::UnaryOp::Neg:
        {
            //   if arg is Num(n) then const(-n)
            if (auto num_arg = dynamic_cast<AST::Num*>(n->exp.get())) {
                // const(-n)
                m_last_result_id = const_var(-static_cast<int>(num_arg->value));
            } else {
                // else
                //   let lhs = fresh_non_inner_var(Int)
                LIR::VarId lhs = fresh_non_inner_var(std::make_shared<LIR::IntType>());
                //   let zero = const(0)
                LIR::VarId zero = const_var(0);
                //   let x = ⟦arg⟧ᵉ
                LIR::VarId x = lower_exp(n->exp.get());
                //   % Arith(lhs, Sub, zero, x)
                m_tv.push_back(LIR::Arith{lhs, LIR::ArithOp::Sub, zero, x});
                //   release([x])
                release({x});
                //   lhs
                m_last_result_id = lhs;
            }
            break;
        }
        case AST::UnaryOp::Not:
        {
            // ⟦UnOp(Not, arg)⟧ᵉ = ⟦BinOp(Eq, arg, Num(0))⟧ᵉ
            auto zero_num = std::make_unique<AST::Num>(0);
            auto eq_binop = std::make_unique<AST::BinOp>(AST::BinaryOp::Eq, std::move(n->exp), std::move(zero_num));
            // Call visit on the new BinOp
            visit(eq_binop.get());
            // m_last_result_id is already set by visit(AST::BinOp*)
            break;
        }
    }
}

void Lowerer::visit(AST::BinOp* n) {
    switch(n->op) {
        case AST::BinaryOp::Add:
        case AST::BinaryOp::Sub:
        case AST::BinaryOp::Mul:
        case AST::BinaryOp::Div:
        {
            // ⟦BinOp(op ∈ {Add,Sub,Mul,Div}, left, right)⟧ᵉ =
            //   let op1 = ⟦left⟧ᵉ
            LIR::VarId op1 = lower_exp(n->left.get());
            //   let op2 = ⟦right⟧ᵉ
            LIR::VarId op2 = lower_exp(n->right.get());
            //   let lhs = fresh_non_inner_var(Int)
            LIR::VarId lhs = fresh_non_inner_var(std::make_shared<LIR::IntType>());
            //   % Arith(lhs, op, op1, op2)
            m_tv.push_back(LIR::Arith{lhs, convert_arith_op(n->op), op1, op2});
            //   release([op1, op2])
            release({op1, op2});
            //   lhs
            m_last_result_id = lhs;
            break;
        }

        case AST::BinaryOp::Eq:
        case AST::BinaryOp::NotEq:
        case AST::BinaryOp::Lt:
        case AST::BinaryOp::Lte:
        case AST::BinaryOp::Gt:
        case AST::BinaryOp::Gte:
        {
            // ⟦BinOp(op ∈ {Eq, NotEq, Lt, Lte, Gt, Gte}, left, right)⟧ᵉ =
            //   let op1 = ⟦left⟧ᵉ
            LIR::VarId op1 = lower_exp(n->left.get());
            //   let op2 = ⟦right⟧ᵉ
            LIR::VarId op2 = lower_exp(n->right.get());
            //   let lhs = fresh_non_inner_var(Int)
            LIR::VarId lhs = fresh_non_inner_var(std::make_shared<LIR::IntType>());
            //   % Cmp(lhs, op, op1, op2)
            m_tv.push_back(LIR::Cmp{lhs, convert_rel_op(n->op), op1, op2});
            //   release([op1, op2])
            release({op1, op2});
            //   lhs
            m_last_result_id = lhs;
            break;
        }

        case AST::BinaryOp::And:
        {
            // ⟦BinOp(And, left, right)⟧ᵉ = ⟦Select(left, right, Num(0))⟧ᵉ
            //     ⟦Select(g, tt, ff)⟧ᵉ =
            //   let TT = label(), FF = label(), END = label()
            auto zero_num = std::make_unique<AST::Num>(0);
            LIR::BbId TT = new_label("and_true");
            LIR::BbId FF = new_label("and_false");
            LIR::BbId END = new_label("and_end");
            //   let x = Id("__NULL")
            LIR::VarId x = "__NULL";
            //   let y = ⟦g⟧ᵉ
            LIR::VarId y = lower_exp(n->left.get());
            //   % Branch(y, TT, FF)
            m_tv.push_back(LIR::Branch{y, TT, FF});
            //   % Label(TT)
            m_tv.push_back(TvLabel{TT});
            //   release([y])
            release({y});
            //   let z = ⟦tt⟧ᵉ
            LIR::VarId z = lower_exp(n->right.get());
            //   if z != Id("__NULL"):
            //     x = fresh_non_inner_var(typeof(z))
            //     % Copy(x, z)
            if (z != "__NULL") {
                LIR::TypePtr z_type = typeof_var(z);
                x = fresh_non_inner_var(z_type);
                m_tv.push_back(LIR::Copy{x, z});
            }
            //   release([z])
            release({z});
            //   % Jump(END)  
            m_tv.push_back(LIR::Jump{END});
            //   % Label(FF)
            m_tv.push_back(TvLabel{FF});
            //   let w = ⟦ff⟧ᵉ
            LIR::VarId w = lower_exp(zero_num.get());
            //   if w != Id("__NULL"):
            //     if x == Id("__NULL"):
            //       x = fresh_non_inner_var(typeof(w))
            //     % Copy(x, w)
            if (w != "__NULL") {
                if (x == "__NULL") {
                    LIR::TypePtr w_type = typeof_var(w);
                    x = fresh_non_inner_var(w_type);
                }
                m_tv.push_back(LIR::Copy{x, w});
            }
            //   release([w])
            release({w});
            //   % Jump(END)
            m_tv.push_back(LIR::Jump{END});
            //   % Label(END)
            m_tv.push_back(TvLabel{END});
            //   x
            m_last_result_id = x;

            break;
        }

        case AST::BinaryOp::Or:
        {
            //  ⟦BinOp(Or, left, right)⟧ᵉ =
            //   let FF = label(), END = label()
            LIR::BbId FF = new_label("or_false");
            LIR::BbId END = new_label("or_end");
            //   let x = ⟦left⟧ᵉ
            LIR::VarId x = lower_exp(n->left.get());
            //   let y = fresh_non_inner_var(Int)
            LIR::VarId y = fresh_non_inner_var(std::make_shared<LIR::IntType>());
            //   % Copy(y, x)
            m_tv.push_back(LIR::Copy{y, x});
            //   % Branch(y, END, FF)
            m_tv.push_back(LIR::Branch{y, END, FF});
            //   % Label(FF)
            m_tv.push_back(TvLabel{FF});
            //   release([x])
            release({x});
            //   let z = ⟦ff⟧ᵉ
            LIR::VarId z = lower_exp(n->right.get());
            //   % Copy(y, z)
            m_tv.push_back(LIR::Copy{y, z});
            //   release([z])
            release({z});
            //   % Jump(END)
            m_tv.push_back(LIR::Jump{END});
            //   % Label(END)
            m_tv.push_back(TvLabel{END});
            //   y
            m_last_result_id = y;
            break;
        }
    }
}

void Lowerer::visit(AST::NewSingle* n) {
    // ⟦NewSingle(τ)⟧ᵉ
    //   let lhs = fresh_non_inner_var(Ptr(τ))
    LIR::TypePtr tau = convert_type(n->type);
    LIR::TypePtr ptr_tau = std::make_shared<LIR::PtrType>(tau);
    LIR::VarId lhs = fresh_non_inner_var(ptr_tau);
    
    //   % AllocSingle(lhs, τ)
    m_tv.push_back(LIR::AllocSingle{lhs, tau});
    
    //   lhs
    m_last_result_id = lhs;
}

void Lowerer::visit(AST::NewArray* n) {
    //     ⟦NewArray(τ, amt)⟧ᵉ =
    //   let lhs = fresh_non_inner_var(Array(τ))
    //   let x = ⟦amt⟧ᵉ
    //   % AllocArray(lhs, x, τ)
    //   release([x])
    //   lhs
    LIR::TypePtr tau = convert_type(n->type);
    LIR::TypePtr arr_tau = std::make_shared<LIR::ArrayType>(tau);
    LIR::VarId lhs = fresh_non_inner_var(arr_tau);
    LIR::VarId x = lower_exp(n->size.get());
    m_tv.push_back(LIR::AllocArray{lhs, x, tau});
    release({x});
    m_last_result_id = lhs;
}

void Lowerer::visit(AST::CallExp* n) {
    // This is just a wrapper for FunCall
    // Call visit on the raw pointer
    visit(n->fun_call.get());
    // m_last_result_id is already set by visit(AST::FunCall*)
}

// --- Visitor Implementations: Place ---
// These are called by `lower_place`

void Lowerer::visit(AST::Id* n) {
    // Id should never be lowered as a place (to get its address).
    // When Id appears on the LHS of an assignment, it's handled specially
    // in visit(AST::Assign*) without calling lower_place.
    // When Id appears as an expression value, it's wrapped in Val and
    // handled in visit(AST::Val*) without calling lower_place.
    (void)n;
    throw std::runtime_error("Id should not be lowered as a place (address). This indicates a bug in the lowerer.");
}

void Lowerer::visit(AST::Deref* n) {
    // ⟦Deref(e)⟧ˡ = ⟦e⟧ᵉ
    m_last_result_id = lower_exp(n->exp.get());
}

void Lowerer::visit(AST::ArrayAccess* n) {
    // ⟦ArrayAccess(arr, index)⟧ˡ =
    //   let src = ⟦arr⟧ᵉ
    LIR::VarId src = lower_exp(n->array.get());
    //   let idx = ⟦index⟧ᵉ
    LIR::VarId idx = lower_exp(n->index.get());
    
    //   let lhs = fresh_inner_var(Ptr(τ)) s.t. typeof(src) = Array(τ)
    LIR::TypePtr arr_type = typeof_var(src);
    LIR::TypePtr elem_type = typeof_array_element(arr_type);
    LIR::TypePtr ptr_elem_type = std::make_shared<LIR::PtrType>(elem_type);
    LIR::VarId lhs = fresh_inner_var(ptr_elem_type);
    
    //   % Gep(lhs, src, idx, true)
    m_tv.push_back(LIR::Gep{lhs, src, idx, true});
    
    //   release([src, idx])
    release({src, idx});
    
    //   lhs
    m_last_result_id = lhs;
}

void Lowerer::visit(AST::FieldAccess* n) {
    //     ⟦FieldAccess(ptr, fld)⟧ˡ =
    //   let src = ⟦ptr⟧ᵉ
    //   let sid = id s.t. typeof(src) = Ptr(Struct(id))
    //   let lhs = fresh_inner_var(Ptr(typeof(sid[fld])))
    //   % Gfp(lhs, src, sid, fld)
    //   release([src])
    //   lhs
    LIR::VarId src = lower_exp(n->ptr.get());
    LIR::TypePtr ptr_type = typeof_var(src);
    LIR::TypePtr struct_type = typeof_ptr_element(ptr_type);
    auto struct_type_cast = std::dynamic_pointer_cast<LIR::StructType>(struct_type);
    if (!struct_type_cast) {
        throw std::runtime_error("FieldAccess on non-struct pointer type");
    }
    LIR::VarId sid = fresh_inner_var(ptr_type);
    LIR::VarId lhs = fresh_inner_var(std::make_shared<LIR::PtrType>(typeof_struct_field(struct_type_cast, n->field)));
    m_tv.push_back(LIR::Gfp{lhs, src, sid, n->field});
    release({src});
    m_last_result_id = lhs;
}

// --- Visitor Implementations: Other ---

void Lowerer::visit(AST::FunCall* n) {
    // This is called by CallStmt and CallExp
    
    // ⟦FunCall(callee, args)⟧ᵉ =
    //   let xs = ∀a ∈ args.⟦a⟧ᵉ (in reverse order)
    std::vector<LIR::VarId> xs;
    for (auto it = n->args.rbegin(); it != n->args.rend(); ++it) {
        xs.push_back(lower_exp(it->get()));
    }
    
    //   let fun = ⟦callee⟧ᵉ
    LIR::VarId fun = lower_exp(n->callee.get());
    
    //   let lhs = fresh_non_inner_var(τ) s.t. typeof(fun) ∈ {Fn(_,τ), Ptr(Fn(_,τ))}
    LIR::TypePtr fun_type = typeof_var(fun);
    LIR::TypePtr ret_type = typeof_func_ret(fun_type);
    LIR::VarId lhs = fresh_non_inner_var(ret_type);
    
    //   % Call(lhs, fun, xs)  -- Note: lower.md has 'zs' here, typo for 'xs'
    m_tv.push_back(LIR::Call{lhs, fun, xs});
    
    //   release(xs ++ [fun])
    xs.push_back(fun);
    release(xs);
    
    //   lhs
    m_last_result_id = lhs;
}

// --- Helper Function Implementations ---

LIR::VarId Lowerer::lower_exp(AST::Exp* exp) {
    exp->accept(*this);
    return m_last_result_id;
}

LIR::VarId Lowerer::lower_place(AST::Place* place) {
    // Places are Nodes, but not Exps, so we need to visit them directly
    if (auto p = dynamic_cast<AST::Id*>(place))         { visit(p); }
    else if (auto p = dynamic_cast<AST::Deref*>(place)) { visit(p); }
    else if (auto p = dynamic_cast<AST::ArrayAccess*>(place)) { visit(p); }
    else if (auto p = dynamic_cast<AST::FieldAccess*>(place)) { visit(p); }
    else { throw std::runtime_error("Unknown Place type"); }
    return m_last_result_id;
}

void Lowerer::lower_stmt(AST::Stmt* stmt) {
    stmt->accept(*this);
}

LIR::VarId Lowerer::fresh_inner_var(LIR::TypePtr type) {
    // ⟦fresh_inner_var(τ)⟧
    // TODO: Implement var reuse
    LIR::VarId name = "_inner" + std::to_string(m_tmp_counter++);
    m_current_fun->locals[name] = type;
    return name;
}

LIR::VarId Lowerer::fresh_non_inner_var(LIR::TypePtr type) {
    // ⟦fresh_non_inner_var(τ)⟧
    // TODO: Implement var reuse
    LIR::VarId name = "_tmp" + std::to_string(m_tmp_counter++);
    m_current_fun->locals[name] = type;
    return name;
}

void Lowerer::release(std::vector<LIR::VarId> vars) {
    // ⟦release([op...])⟧
    // TODO: Implement var reuse by adding vars to a free-list
    (void)vars; // Suppress unused warning
}

LIR::VarId Lowerer::const_var(int n) {
    // ⟦const(n)⟧
    std::string name = "_const_" + (n < 0 ? "n" + std::to_string(-n) : std::to_string(n));
    
    if (m_current_fun->locals.find(name) == m_current_fun->locals.end()) {
        // Not found, create it
        m_current_fun->locals[name] = std::make_shared<LIR::IntType>();
        
        // Insert $const instruction at the tracked position (after entry label, before other constants)
        // This ensures constants stay in order and at the top of the entry block
        m_tv.insert(m_tv.begin() + m_const_insert_pos, LIR::Const{name, n});
        m_const_insert_pos++; // Increment so next const goes after this one
    }
    return name;
}

LIR::BbId Lowerer::new_label(const std::string& prefix) {
    // ⟦label()⟧
    return prefix + std::to_string(m_label_counter++);
}

LIR::TypePtr Lowerer::typeof_var(LIR::VarId id) {
    // ⟦typeof(x)⟧
    // 1. Check locals
    if (m_current_fun) {
        auto it = m_current_fun->locals.find(id);
        if (it != m_current_fun->locals.end()) {
            return it->second;
        }
    }
    // 2. Check params (already in locals)
    
    // 3. Check funptrs
    auto it_fp = m_lir_prog->funptrs.find(id);
    if (it_fp != m_lir_prog->funptrs.end()) {
        return it_fp->second;
    }
    
    // 4. Check externs
    auto it_ex = m_lir_prog->externs.find(id);
    if (it_ex != m_lir_prog->externs.end()) {
        return it_ex->second;
    }

    if (id == "__NULL") {
        return std::make_shared<LIR::NilType>();
    }
    
    std::cerr << "Warning: Could not find type for VarId: " << id << std::endl;
    // throw std::runtime_error("Could not find type for VarId: " + id);
    // Return a dummy type to avoid crashing, though this indicates an error
    return std::make_shared<LIR::IntType>(); // Or some error type
}

LIR::TypePtr Lowerer::typeof_field(LIR::StructId sid, LIR::FieldId fid) {
    if (m_lir_prog->structs.count(sid)) {
        const auto& s = m_lir_prog->structs.at(sid);
        if (s.fields.count(fid)) {
            return s.fields.at(fid);
        }
    }
    throw std::runtime_error("Could not find field " + fid + " in struct " + sid);
}

LIR::TypePtr Lowerer::typeof_array_element(LIR::TypePtr arr_type) {
    if (auto at = std::dynamic_pointer_cast<LIR::ArrayType>(arr_type)) {
        return at->element;
    }
    throw std::runtime_error("Type is not an array type");
}

LIR::TypePtr Lowerer::typeof_ptr_element(LIR::TypePtr ptr_type) {
    if (auto pt = std::dynamic_pointer_cast<LIR::PtrType>(ptr_type)) {
        return pt->element;
    }
    throw std::runtime_error("Type is not a pointer type");
}

LIR::TypePtr Lowerer::typeof_func_ret(LIR::TypePtr fn_type) {
    // Check for Fn(_,τ)
    if (auto ft = std::dynamic_pointer_cast<LIR::FnType>(fn_type)) {
        return ft->ret;
    }
    // Check for Ptr(Fn(_,τ))
    if (auto pt = std::dynamic_pointer_cast<LIR::PtrType>(fn_type)) {
        if (auto ft_inner = std::dynamic_pointer_cast<LIR::FnType>(pt->element)) {
            return ft_inner->ret;
        }
    }
    throw std::runtime_error("Type is not a function or function pointer type");
}


// --- Type Conversion Helper ---

LIR::TypePtr Lowerer::convert_type(const std::shared_ptr<AST::Type>& ast_type) {
    // 🔴 YOUR TASK: Implement this based on your ast.hpp types
    if (dynamic_cast<AST::IntType*>(ast_type.get())) {
        return std::make_shared<LIR::IntType>();
    }
    if (dynamic_cast<AST::NilType*>(ast_type.get())) {
        return std::make_shared<LIR::NilType>();
    }
    if (auto s = dynamic_cast<AST::StructType*>(ast_type.get())) {
        return std::make_shared<LIR::StructType>(s->name);
    }
    if (auto p = dynamic_cast<AST::PtrType*>(ast_type.get())) {
        return std::make_shared<LIR::PtrType>(convert_type(p->pointeeType));
    }
    if (auto a = dynamic_cast<AST::ArrayType*>(ast_type.get())) {
        return std::make_shared<LIR::ArrayType>(convert_type(a->elementType));
    }
    if (auto f = dynamic_cast<AST::FnType*>(ast_type.get())) {
        std::vector<LIR::TypePtr> params;
        for (const auto& p : f->paramTypes) {
            params.push_back(convert_type(p));
        }
        return std::make_shared<LIR::FnType>(params, convert_type(f->returnType));
    }
    
    throw std::runtime_error("Unknown AST::Type to convert to LIR::Type");
}

LIR::ArithOp Lowerer::convert_arith_op(AST::BinaryOp op) {
    switch(op) {
        case AST::BinaryOp::Add: return LIR::ArithOp::Add;
        case AST::BinaryOp::Sub: return LIR::ArithOp::Sub;
        case AST::BinaryOp::Mul: return LIR::ArithOp::Mul;
        case AST::BinaryOp::Div: return LIR::ArithOp::Div;
        default: throw std::runtime_error("Not an arith op");
    }
}

LIR::RelOp Lowerer::convert_rel_op(AST::BinaryOp op) {
     switch(op) {
        case AST::BinaryOp::Eq:    return LIR::RelOp::Eq;
        case AST::BinaryOp::NotEq: return LIR::RelOp::NotEq;
        case AST::BinaryOp::Lt:    return LIR::RelOp::Lt;
        case AST::BinaryOp::Lte:   return LIR::RelOp::Lte;
        case AST::BinaryOp::Gt:    return LIR::RelOp::Gt;
        case AST::BinaryOp::Gte:   return LIR::RelOp::Gte;
        default: throw std::runtime_error("Not a rel op");
    }
}


// --- Pass 2: TV -> CFG (Implemented for you) ---

void Lowerer::build_cfg() {
    LIR::BasicBlock* current_bb = nullptr;
    
    for (const auto& item : m_tv) {
        if (std::holds_alternative<TvLabel>(item)) {
            // This is a Label. It starts a new BasicBlock.
            const auto& label = std::get<TvLabel>(item);
            
            // Get or create the block in the function's body
            current_bb = &m_current_fun->body[label.name];
            current_bb->label = label.name;
        }
        else if (std::holds_alternative<LIR::Inst>(item)) {
            // This is a regular instruction.
            if (!current_bb) {
                // This can happen for $const instructions added to the top
                current_bb = &m_current_fun->body.at(m_current_fun->name + "_entry");
            }
            current_bb->insts.push_back(std::get<LIR::Inst>(item));
        }
        else if (std::holds_alternative<LIR::Terminal>(item)) {
            // This is a terminal. It ends the current BasicBlock.
            if (!current_bb) {
                 std::cerr << "Warning: Terminal without a preceding label.\n";
                 continue;
            }
            current_bb->term = std::get<LIR::Terminal>(item);
            current_bb = nullptr; // Reset, wait for the next label
        }
    }
    
    // TODO: Remove unreachable basic blocks (as per lower.md)
}