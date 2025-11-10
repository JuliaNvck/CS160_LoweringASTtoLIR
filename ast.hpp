#ifndef AST_HPP
#define AST_HPP

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <set>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include "json.hpp"

#include "ast_visitor.hpp"

// Put AST node types into namespace AST
namespace AST {

// Type Representation
// Defines the structure of types in Cflat (int, struct, ptr, etc.)

// Type equality function eq(τ₁, τ₂) handling nil - Forward Declaration
bool typeEq(const std::shared_ptr<struct Type>& t1, const std::shared_ptr<struct Type>& t2);

// Base class for all Cflat types
struct Type {
    virtual ~Type() = default;
    virtual std::string toString() const = 0;
    // Overload == for easier type comparison, especially with NilType
    virtual bool equals(const Type& other) const = 0;
    // Print method for output streams
    virtual void print(std::ostream& os) const {
        os << toString();
    }
};

// Custom comparison for shared_ptr<Type>
struct TypePtrEqual {
    bool operator()(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs) const {
        if (!lhs && !rhs) return true;
        if (!lhs || !rhs) return false;
        return lhs->equals(*rhs);
    }
};

struct IntType : Type {
    std::string toString() const override { return "int"; }
    bool equals(const Type& other) const override {
        return dynamic_cast<const IntType*>(&other) != nullptr;
    }
};

struct NilType : Type {
    std::string toString() const override { return "nil"; }
    bool equals(const Type& other) const override;
};

struct StructType : Type {
    std::string name;
    StructType(std::string n) : name(std::move(n)) {}
    ~StructType() override;
    std::string toString() const override { return name; }
    bool equals(const Type& other) const override;
};

struct ArrayType : Type {
    std::shared_ptr<Type> elementType;
    ArrayType(std::shared_ptr<Type> et) : elementType(std::move(et)) {}
    ~ArrayType() override;
    std::string toString() const override { 
        if (elementType) {
        // Wrap the element type string in brackets
        return "[" + elementType->toString() + "]";
    } else {
        return "[<null>]";
    }
     }
    bool equals(const Type& other) const override;
};

struct PtrType : Type {
    std::shared_ptr<Type> pointeeType;
    PtrType(std::shared_ptr<Type> pt) : pointeeType(std::move(pt)) {}
    ~PtrType() override;
    std::string toString() const override { 
        if (pointeeType) {
            return "&" + pointeeType->toString();
        } else {
            return "ptr(<null>)";
        }
    }
    bool equals(const Type& other) const override;
};

struct FnType : Type {
    std::vector<std::shared_ptr<Type>> paramTypes;
    std::shared_ptr<Type> returnType;
    FnType(std::vector<std::shared_ptr<Type>> pt, std::shared_ptr<Type> rt)
        : paramTypes(std::move(pt)), returnType(std::move(rt)) {}
    ~FnType() override;
    std::string toString() const override;
    bool equals(const Type& other) const override;
};

// Type equality function eq(τ₁, τ₂) implementation
// compares types s.t. two types are eq iff they are the same
// type or if one is a pointer or array type and the other is nil.
bool typeEq(const std::shared_ptr<Type>& t1, const std::shared_ptr<Type>& t2);

// pick_nonnil helper function
// Type × Type → Type that takes two types and returns one of the
// arguments that isn’t nil if possible; if both arguments are nil then it returns nil.
std::shared_ptr<Type> pickNonNil(const std::shared_ptr<Type>& t1, const std::shared_ptr<Type>& t2);

// Symbol Tables (Environments)
// Data structures to hold type information during checking

// Γ: Id → Type (Variables and Function names to Types)
using Gamma = std::unordered_map<std::string, std::shared_ptr<Type>>;

// Δ: Id → (Id → Type) (Struct names to [Field names to Types])
using Delta = std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<Type>>>;

// Error Handling
class TypeError : public std::runtime_error {
public:
    // Store line/col info if available from JSON later
    TypeError(const std::string& message) : std::runtime_error(message) {}
};

// AST Node Representation

// Base class for all AST nodes
struct Node {
    virtual ~Node() = default;
    virtual void print(std::ostream& os) const = 0;
    virtual void accept(::ASTVisitor& v) = 0;
};

} // namespace AST

// Overload << operator to make printing easy (work with AST:: types)
inline std::ostream& operator<<(std::ostream& os, const AST::Node& node) {
    node.print(os);
    return os;
}
inline std::ostream& operator<<(std::ostream& os, const std::unique_ptr<AST::Node>& node) {
    if (node) node->print(os); else os << "<null>";
    return os;
}
inline std::ostream& operator<<(std::ostream& os, const std::shared_ptr<AST::Type>& type) {
    if (type) type->print(os); else os << "<null>";
    return os;
}

template <class T,
          class = std::enable_if_t<std::is_base_of_v<AST::Node, T>>>
inline std::ostream& operator<<(std::ostream& os,
                                const std::unique_ptr<T>& p) {
    if (p) p->print(os); else os << "<null>";
    return os;
}

// Put remaining AST node definitions into namespace AST
namespace AST {

// Declarations (parameters, locals, struct fields)
struct Decl : public Node {
    std::string name;
    std::shared_ptr<Type> type;

    Decl(std::string n, std::shared_ptr<Type> t) : name(std::move(n)), type(std::move(t)) {}
    void print(std::ostream& os) const override {
        os << "Decl { name: \"" << name << "\", typ: ";
        os << type;
        os << " }";
    }
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

// Expressions
enum class UnaryOp { Neg, Not };
enum class BinaryOp { Add, Sub, Mul, Div, And, Or, Eq, NotEq, Lt, Lte, Gt, Gte };

// Base class for expressions
struct Exp : public Node {
    // Check method: Returns the type of the expression or throws TypeError
    virtual std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const = 0;
    // Helper to get string representation for error messages
    virtual std::string toString() const = 0;
};

// Base class for places
struct Place : public Node {
     // Check method for Places returns the type they refer to
    virtual std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const = 0;
    virtual std::string toString() const = 0;
};

// Specific Node Implementations

struct Id : public Place {
    std::string name;
    explicit Id(std::string n) : name(std::move(n)) {}
    ~Id() override;
    void print(std::ostream& os) const override { os << "Id(\"" << name << "\")"; }
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
    std::string toString() const override { return "Id(\"" + name + "\")"; }
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

// Val wraps a Place when used as an expression
struct Val : public Exp {
    std::unique_ptr<Place> place;
    explicit Val(std::unique_ptr<Place> p) : place(std::move(p)) {}
    void print(std::ostream& os) const override { os << "Val(" << *place << ")"; }
    // Check delegates to the Place's check
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override { return place->check(gamma, delta); }
     std::string toString() const override { return "Val(" + place->toString() + ")";}
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct Num : public Exp {
    long long value;
    explicit Num(long long val) : value(val) {}
    ~Num() override;
    void print(std::ostream& os) const override { os << "Num(" << value << ")"; }
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
     std::string toString() const override { return "Num(" + std::to_string(value) + ")"; }
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct NilExp : public Exp {
    void print(std::ostream& os) const override { os << "Nil"; }
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
     std::string toString() const override { return "nil"; }
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct Select : public Exp {
    std::unique_ptr<Exp> guard;
    std::unique_ptr<Exp> tt;
    std::unique_ptr<Exp> ff;
    Select(std::unique_ptr<Exp> g, std::unique_ptr<Exp> t, std::unique_ptr<Exp> f)
    : guard(std::move(g)), tt(std::move(t)), ff(std::move(f)) {}
    void print(std::ostream& os) const override { os << "Select { guard: " << guard << ", tt: " << tt << ", ff: " << ff << " }"; }
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
    std::string toString() const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct UnOp : public Exp {
    UnaryOp op;
    std::unique_ptr<Exp> exp;
    UnOp(UnaryOp o, std::unique_ptr<Exp> e) : op(o), exp(std::move(e)) {}
    void print(std::ostream& os) const override;
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
    std::string toString() const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct BinOp : public Exp {
    BinaryOp op;
    std::unique_ptr<Exp> left;
    std::unique_ptr<Exp> right;
    BinOp(BinaryOp o, std::unique_ptr<Exp> l, std::unique_ptr<Exp> r)
    : op(o), left(std::move(l)), right(std::move(r)) {}
    ~BinOp() override;
    void print(std::ostream& os) const override;
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
    std::string toString() const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct NewSingle : public Exp {
    std::shared_ptr<Type> type;
    explicit NewSingle(std::shared_ptr<Type> t) : type(std::move(t)) {}
    //void print(std::ostream& os) const override { os << "NewSingle(" << type << ")"; }
    void print(std::ostream& os) const override;
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
    //std::string toString() const override { return "new " + type->toString(); }
    std::string toString() const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct NewArray : public Exp {
    std::shared_ptr<Type> type;
    std::unique_ptr<Exp> size;
    NewArray(std::shared_ptr<Type> t, std::unique_ptr<Exp> s)
    : type(std::move(t)), size(std::move(s)) {}
    //void print(std::ostream& os) const override { os << "NewArray(" << type << ", " << size << ")"; }
    void print(std::ostream& os) const override;
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
    std::string toString() const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct Deref : public Place {
    std::unique_ptr<Exp> exp;
    explicit Deref(std::unique_ptr<Exp> e) : exp(std::move(e)) {}
    ~Deref() override;
    void print(std::ostream& os) const override { os << "Deref(" << exp << ")"; }
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
    std::string toString() const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct ArrayAccess : public Place {
    std::unique_ptr<Exp> array;
    std::unique_ptr<Exp> index;
    ArrayAccess(std::unique_ptr<Exp> arr, std::unique_ptr<Exp> idx)
    : array(std::move(arr)), index(std::move(idx)) {}
    ~ArrayAccess() override;
    void print(std::ostream& os) const override { os << "ArrayAccess { array: " << array << ", idx: " << index << " }"; }
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
     std::string toString() const override; // { return array->toString() + "[" + index->toString() + "]"; }
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct FieldAccess : public Place {
    std::unique_ptr<Exp> ptr;
    std::string field;
    FieldAccess(std::unique_ptr<Exp> p, std::string f)
    : ptr(std::move(p)), field(std::move(f)) {}
    ~FieldAccess() override;
    void print(std::ostream& os) const override { os << "FieldAccess { ptr: " << ptr << ", field: \"" << field << "\" }"; }
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override;
    std::string toString() const override; //  { return ptr->toString() + "." + field; }
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct FunCall: Node {
    std::unique_ptr<Exp> callee;
    std::vector<std::unique_ptr<Exp>> args;
    FunCall(std::unique_ptr<Exp> c, std::vector<std::unique_ptr<Exp>> a)
    : callee(std::move(c)), args(std::move(a)) {}
    void print(std::ostream& os) const override;
    // FunCall itself doesn't have a type, CallExp does.
    // However, we might need a helper check method here or do it all in CallExp::check
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const; // Added check here
    std::string toString() const; // Added toString
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct CallExp : public Exp {
    std::unique_ptr<FunCall> fun_call;
    explicit CallExp(std::unique_ptr<FunCall> fc) : fun_call(std::move(fc)) {}
    void print(std::ostream& os) const override { os << "Call(" << fun_call << ")"; }
    std::shared_ptr<Type> check(const Gamma& gamma, const Delta& delta) const override { return fun_call->check(gamma, delta); }
    std::string toString() const override { return "Call(" + fun_call->toString() + ")"; }
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

// Statement nodes
struct Stmt : public Node {
     // Check method: Returns true if the statement definitely executes a return
    virtual bool check(const Gamma& gamma, const Delta& delta, const std::shared_ptr<Type>& returnType, bool inLoop) const = 0;
};

struct Stmts : public Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;
    void print(std::ostream& os) const override {
        os << "[";
        for (size_t i = 0; i < statements.size(); ++i) {
            os << statements[i];
            if (i < statements.size() - 1) os << ", ";
        }
        os << "]";
    }
    bool check(const Gamma& gamma, const Delta& delta, const std::shared_ptr<Type>& returnType, bool inLoop) const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct Assign : public Stmt {
    std::unique_ptr<Place> place;
    std::unique_ptr<Exp> exp;
    Assign(std::unique_ptr<Place> p, std::unique_ptr<Exp> e)
    : place(std::move(p)), exp(std::move(e)) {}
    void print(std::ostream& os) const override { os << "Assign(" << place << ", " << exp << ")"; }
    bool check(const Gamma& gamma, const Delta& delta, const std::shared_ptr<Type>& returnType, bool inLoop) const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct CallStmt : public Stmt {
    std::unique_ptr<FunCall> fun_call;
    explicit CallStmt(std::unique_ptr<FunCall> fc) : fun_call(std::move(fc)) {}
    void print(std::ostream& os) const override { os << "Call(" << fun_call << ")"; }
    bool check(const Gamma& gamma, const Delta& delta, const std::shared_ptr<Type>& returnType, bool inLoop) const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct If : public Stmt {
    std::unique_ptr<Exp> guard;
    std::unique_ptr<Stmt> tt;
    std::optional<std::unique_ptr<Stmt>> ff;

    If(std::unique_ptr<Exp> g, std::unique_ptr<Stmt> t, std::optional<std::unique_ptr<Stmt>> f)
    : guard(std::move(g)), tt(std::move(t)), ff(std::move(f)) {}
    void print(std::ostream& os) const override;
    bool check(const Gamma& gamma, const Delta& delta, const std::shared_ptr<Type>& returnType, bool inLoop) const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct While : public Stmt {
    std::unique_ptr<Exp> guard;
    std::unique_ptr<Stmt> body;

    While(std::unique_ptr<Exp> g, std::unique_ptr<Stmt> b)
    : guard(std::move(g)), body(std::move(b)) {}
    void print(std::ostream& os) const override { os << "While(" << guard << ", " << body << ")"; }
    bool check(const Gamma& gamma, const Delta& delta, const std::shared_ptr<Type>& returnType, bool inLoop) const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct Break : public Stmt {
    void print(std::ostream& os) const override { os << "Break"; }
    bool check(const Gamma& gamma, const Delta& delta, const std::shared_ptr<Type>& returnType, bool inLoop) const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct Continue : public Stmt {
    void print(std::ostream& os) const override { os << "Continue"; }
    bool check(const Gamma& gamma, const Delta& delta, const std::shared_ptr<Type>& returnType, bool inLoop) const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct Return : public Stmt {
    // Making expression optional to handle potential void returns
    std::optional<std::unique_ptr<Exp>> exp;
    explicit Return(std::optional<std::unique_ptr<Exp>> e) : exp(std::move(e)) {}
    void print(std::ostream& os) const override {
        os << "Return(";
        if(exp) os << (*exp); else os << "<void>";
        os << ")";
    }
    bool check(const Gamma& gamma, const Delta& delta, const std::shared_ptr<Type>& returnType, bool inLoop) const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

// Top level nodes
struct StructDef : public Node {
    std::string name;
    std::vector<Decl> fields;
    void print(std::ostream& os) const override;
    void check(const Gamma& gamma, const Delta& delta) const;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

struct Extern : public Node {
    std::string name;
    std::vector<std::shared_ptr<Type>> param_types;
    std::shared_ptr<Type> rettype;
    void print(std::ostream& os) const override;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};


struct FunctionDef : public Node {
    std::string name;
    std::vector<Decl> params;
    std::shared_ptr<Type> rettype;
    std::vector<Decl> locals;
    std::unique_ptr<Stmt> body;

    void print(std::ostream& os) const override;
    void check(const Gamma& gamma, const Delta& delta) const;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};


struct Program : public Node {
    std::vector<std::unique_ptr<StructDef>> structs;
    std::vector<Extern> externs;
    std::vector<std::unique_ptr<FunctionDef>> functions;

    void print(std::ostream& os) const override;
    void check() const;
    void accept(::ASTVisitor& v) override { v.visit(this); }  
};

} // namespace AST

// --- JSON to AST Conversion ---
// Forward declarations for functions needed to build the AST from JSON
std::shared_ptr<AST::Type> buildType(const nlohmann::json& j);
std::unique_ptr<AST::Exp> buildExp(const nlohmann::json& j);
std::unique_ptr<AST::Place> buildPlace(const nlohmann::json& j);
std::unique_ptr<AST::Stmt> buildStmt(const nlohmann::json& j);
AST::Decl buildDecl(const nlohmann::json& j);
std::unique_ptr<AST::FunctionDef> buildFunctionDef(const nlohmann::json& j);
std::unique_ptr<AST::StructDef> buildStructDef(const nlohmann::json& j);
AST::Extern buildExtern(const nlohmann::json& j);
std::unique_ptr<AST::Program> buildProgram(const nlohmann::json& j);
std::unique_ptr<AST::FunCall> buildFunCall(const nlohmann::json& j);


// --- Environment Construction ---
AST::Gamma construct_gamma(const std::vector<AST::Extern>& externs, const std::vector<std::unique_ptr<AST::FunctionDef>>& functions);
AST::Delta construct_delta(const std::vector<std::unique_ptr<AST::StructDef>>& structs);


#endif