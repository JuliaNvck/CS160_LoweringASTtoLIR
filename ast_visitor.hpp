#pragma once

// Forward declare AST node types in namespace AST (no include to avoid circular includes)
namespace AST {
    // Top-level
    struct Program;
    struct StructDef;
    struct FunctionDef;
    // Stmt
    struct Stmts;
    struct Assign;
    struct CallStmt;
    struct If;
    struct While;
    struct Break;
    struct Continue;
    struct Return;
    // Exp
    struct Val;
    struct Num;
    struct NilExp;
    struct Select;
    struct UnOp;
    struct BinOp;
    struct NewSingle;
    struct NewArray;
    struct CallExp;
    // Place
    struct Id;
    struct Deref;
    struct ArrayAccess;
    struct FieldAccess;
    // Others
    struct FunCall; 
    struct Decl;
    struct Extern; 
}

/*
* ASTVisitor interface
*
* This uses the Visitor pattern to traverse the AST. Your Lowerer
* will inherit from this and implement the 'visit' method for each
* node type.
*/
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    // Define a virtual 'visit' function for every concrete AST node
    // Your lowerer.cpp will implement these.
    
    // Top-level
    virtual void visit(AST::Program* n) = 0;
    virtual void visit(AST::StructDef* n) = 0;
    virtual void visit(AST::FunctionDef* n) = 0;
    virtual void visit(AST::Extern* n) = 0;
    virtual void visit(AST::Decl* n) = 0;

    // Stmt
    virtual void visit(AST::Stmts* n) = 0;
    virtual void visit(AST::Assign* n) = 0;
    virtual void visit(AST::CallStmt* n) = 0;
    virtual void visit(AST::If* n) = 0;
    virtual void visit(AST::While* n) = 0;
    virtual void visit(AST::Break* n) = 0;
    virtual void visit(AST::Continue* n) = 0;
    virtual void visit(AST::Return* n) = 0;

    // Exp
    virtual void visit(AST::Val* n) = 0;
    virtual void visit(AST::Num* n) = 0;
    virtual void visit(AST::NilExp* n) = 0;
    virtual void visit(AST::Select* n) = 0;
    virtual void visit(AST::UnOp* n) = 0;
    virtual void visit(AST::BinOp* n) = 0;
    virtual void visit(AST::NewSingle* n) = 0;
    virtual void visit(AST::NewArray* n) = 0;
    virtual void visit(AST::CallExp* n) = 0;
    
    // Place (Note: Val wraps Place, so visiting Val will visit the Place)
    virtual void visit(AST::Id* n) = 0;
    virtual void visit(AST::Deref* n) = 0;
    virtual void visit(AST::ArrayAccess* n) = 0;
    virtual void visit(AST::FieldAccess* n) = 0;

    // Other
    virtual void visit(AST::FunCall* n) = 0;
};