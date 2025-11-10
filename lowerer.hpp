#pragma once

#include "ast_visitor.hpp"
#include "lir.hpp"
#include <map>
#include <set>
#include <string>

// A TranslationItem can be a Label, an Instruction, or a Terminal
struct TvLabel { LIR::BbId name; };
using TranslationItem = std::variant<
    TvLabel,
    LIR::Inst,
    LIR::Terminal
>;

class Lowerer : public ASTVisitor {
public:
    Lowerer() = default;

    // Main entry point
    std::unique_ptr<LIR::Program> lower(AST::Program* ast_prog);

    // --- Visitor Methods ---    
    // Top-level
    void visit(AST::Program* n) override;
    void visit(AST::StructDef* n) override;
    void visit(AST::FunctionDef* n) override;
    void visit(AST::Extern* n) override;
    void visit(AST::Decl* n) override;

    // Stmt
    void visit(AST::Stmts* n) override;
    void visit(AST::Assign* n) override;
    void visit(AST::CallStmt* n) override;
    void visit(AST::If* n) override;
    void visit(AST::While* n) override;
    void visit(AST::Break* n) override;
    void visit(AST::Continue* n) override;
    void visit(AST::Return* n) override;

    // Exp
    void visit(AST::Val* n) override;
    void visit(AST::Num* n) override;
    void visit(AST::NilExp* n) override;
    void visit(AST::Select* n) override;
    void visit(AST::UnOp* n) override;
    void visit(AST::BinOp* n) override;
    void visit(AST::NewSingle* n) override;
    void visit(AST::NewArray* n) override;
    void visit(AST::CallExp* n) override;
    
    // Place (Places are visited by Val, ArrayAccess, FieldAccess, etc.)
    void visit(AST::Id* n) override;
    void visit(AST::Deref* n) override;
    void visit(AST::ArrayAccess* n) override;
    void visit(AST::FieldAccess* n) override;

    // Other
    void visit(AST::FunCall* n) override;


private:
    // --- State ---
    std::unique_ptr<LIR::Program> m_lir_prog;
    LIR::Function* m_current_fun = nullptr;
    std::vector<TranslationItem> m_tv; // The Translation Vector
    
    // The result of the last lowered expression (for ⟦Exp⟧ᵉ ⟶ VarId)
    // or the last lowered place (for ⟦Place⟧ˡ ⟶ VarId)
    LIR::VarId m_last_result_id; 

    // Counters for fresh vars/labels
    int m_label_counter = 0;
    int m_tmp_counter = 0;
    
    // Track where to insert const instructions (after entry label)
    size_t m_const_insert_pos = 1;

    // --- Loop Context ---
    // Stacks to keep track of the current loop's header/end labels
    // for break and continue
    std::vector<LIR::BbId> m_loop_hdr_stack;
    std::vector<LIR::BbId> m_loop_end_stack;

    // --- Helper Functions (from lower.md) ---

    // Wrappers to call accept and get the result
    LIR::VarId lower_exp(AST::Exp* exp);
    LIR::VarId lower_place(AST::Place* place);
    void lower_stmt(AST::Stmt* stmt);

    // ⟦fresh_..._var(τ)⟧
    LIR::VarId fresh_inner_var(LIR::TypePtr type);
    LIR::VarId fresh_non_inner_var(LIR::TypePtr type);

    // ⟦release([op...])⟧
    void release(std::vector<LIR::VarId> vars);

    // ⟦const(n)⟧
    LIR::VarId const_var(int n);

    // ⟦label()⟧
    LIR::BbId new_label(const std::string& prefix = "lbl");

    // ⟦typeof(x)⟧
    LIR::TypePtr typeof_var(LIR::VarId id);
    
    // Helper to get LIR type for AST struct field
    LIR::TypePtr typeof_field(LIR::StructId sid, LIR::FieldId fid);
    
    // Helper to get LIR type for LIR array
    LIR::TypePtr typeof_array_element(LIR::TypePtr arr_type);
    
    // Helper to get LIR type for LIR pointer
    LIR::TypePtr typeof_ptr_element(LIR::TypePtr ptr_type);
    
    // Helper to get function return type
    LIR::TypePtr typeof_func_ret(LIR::TypePtr fn_type);

    // --- Pass 2: TV -> CFG ---
    void build_cfg();
    
    // Helper to convert AST types to LIR types
    LIR::TypePtr convert_type(const std::shared_ptr<AST::Type>& ast_type);
    
    // Helper to convert AST ArithOp to LIR ArithOp
    LIR::ArithOp convert_arith_op(AST::BinaryOp op);
    
    // Helper to convert AST RelOp to LIR RelOp
    LIR::RelOp convert_rel_op(AST::BinaryOp op);
};