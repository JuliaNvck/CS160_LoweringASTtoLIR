#include "ast.hpp"

// This file provides minimal implementations of AST functions needed by the lowerer
// You may already have a more complete ast.cpp from a previous assignment

namespace AST {

// Implement typeEq if needed (stub for now)
bool typeEq(const std::shared_ptr<Type>& t1, const std::shared_ptr<Type>& t2) {
    if (!t1 && !t2) return true;
    if (!t1 || !t2) return false;
    return t1->equals(*t2);
}

// Implement pickNonNil if needed (stub for now)
std::shared_ptr<Type> pickNonNil(const std::shared_ptr<Type>& t1, const std::shared_ptr<Type>& t2) {
    if (dynamic_cast<const NilType*>(t1.get())) return t2;
    if (dynamic_cast<const NilType*>(t2.get())) return t1;
    return t1;
}

// NilType::equals - needed because it references PtrType/ArrayType which come later
bool NilType::equals(const Type& other) const {
    return dynamic_cast<const NilType*>(&other) ||
           dynamic_cast<const PtrType*>(&other) ||
           dynamic_cast<const ArrayType*>(&other);
}

// Provide non-inline destructors to generate vtables for dynamic_cast
// These are needed for RTTI (Run-Time Type Information)
StructType::~StructType() = default;
PtrType::~PtrType() = default;
ArrayType::~ArrayType() = default;
FnType::~FnType() = default;

// Place types
Id::~Id() = default;
Deref::~Deref() = default;
ArrayAccess::~ArrayAccess() = default;
FieldAccess::~FieldAccess() = default;

// Expression types - needed for lowerer's dynamic AST node creation
Num::~Num() = default;
BinOp::~BinOp() = default;

// Stub implementations for Num methods
std::shared_ptr<Type> Num::check(const Gamma& gamma, const Delta& delta) const {
    (void)gamma;
    (void)delta;
    return std::make_shared<IntType>();
}

// Stub implementations for BinOp methods
std::shared_ptr<Type> BinOp::check(const Gamma& gamma, const Delta& delta) const {
    (void)gamma;
    (void)delta;
    return std::make_shared<IntType>();
}

void BinOp::print(std::ostream& os) const {
    os << "BinOp(...)";
}

std::string BinOp::toString() const {
    return "BinOp(...)";
}

// Type equals implementations
bool StructType::equals(const Type& other) const {
    if (dynamic_cast<const NilType*>(&other)) {
        return false;
    }
    if (const StructType* o = dynamic_cast<const StructType*>(&other)) {
        return name == o->name;
    }
    return false;
}

bool ArrayType::equals(const Type& other) const {
    if (dynamic_cast<const NilType*>(&other)) {
        return true;
    }
    if (const ArrayType* o = dynamic_cast<const ArrayType*>(&other)) {
        return elementType->equals(*o->elementType);
    }
    return false;
}

bool PtrType::equals(const Type& other) const {
    if (dynamic_cast<const NilType*>(&other)) {
        return true;
    }
    if (const PtrType* o = dynamic_cast<const PtrType*>(&other)) {
        return pointeeType->equals(*o->pointeeType);
    }
    return false;
}

bool FnType::equals(const Type& other) const {
    if (dynamic_cast<const NilType*>(&other)) {
        return false;
    }
    if (const FnType* o = dynamic_cast<const FnType*>(&other)) {
        if (paramTypes.size() != o->paramTypes.size()) {
            return false;
        }
        for (size_t i = 0; i < paramTypes.size(); i++) {
            if (!paramTypes[i]->equals(*o->paramTypes[i])) {
                return false;
            }
        }
        return returnType->equals(*o->returnType);
    }
    return false;
}

std::string FnType::toString() const {
    std::string result = "fn(";
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) result += ", ";
        result += paramTypes[i]->toString();
    }
    result += ") -> " + returnType->toString();
    return result;
}

// Stub check methods - not used by lowerer
std::shared_ptr<Type> Id::check(const Gamma&, const Delta&) const {
    throw std::runtime_error("Id::check not implemented - not needed for lowerer");
}

std::shared_ptr<Type> Deref::check(const Gamma&, const Delta&) const {
    throw std::runtime_error("Deref::check not implemented - not needed for lowerer");
}

std::shared_ptr<Type> ArrayAccess::check(const Gamma&, const Delta&) const {
    throw std::runtime_error("ArrayAccess::check not implemented - not needed for lowerer");
}

std::shared_ptr<Type> FieldAccess::check(const Gamma&, const Delta&) const {
    throw std::runtime_error("FieldAccess::check not implemented - not needed for lowerer");
}

std::string Deref::toString() const { return "*" + exp->toString(); }
std::string ArrayAccess::toString() const { 
    return array->toString() + "[" + index->toString() + "]"; 
}
std::string FieldAccess::toString() const { return ptr->toString() + "." + field; }

// Minimal print() implementations to generate vtables (only for non-inline ones)
void UnOp::print(std::ostream& os) const { (void)os; }
void FunCall::print(std::ostream& os) const { (void)os; }
void NewSingle::print(std::ostream& os) const { (void)os; }
void NewArray::print(std::ostream& os) const { (void)os; }
void If::print(std::ostream& os) const { (void)os; }
void Extern::print(std::ostream& os) const { (void)os; }
void StructDef::print(std::ostream& os) const { (void)os; }
void FunctionDef::print(std::ostream& os) const { (void)os; }
void Program::print(std::ostream& os) const { (void)os; }

// Non-inline check() methods to satisfy vtables
bool If::check(const Gamma&, const Delta&, const std::shared_ptr<Type>&, bool) const { return false; }
std::shared_ptr<Type> UnOp::check(const Gamma&, const Delta&) const { return nullptr; }
std::shared_ptr<Type> NewSingle::check(const Gamma&, const Delta&) const { return nullptr; }
std::shared_ptr<Type> NewArray::check(const Gamma&, const Delta&) const { return nullptr; }

// toString() methods
std::string UnOp::toString() const { return "UnOp"; }
std::string NewSingle::toString() const { return "NewSingle"; }
std::string NewArray::toString() const { return "NewArray"; }
std::string FunCall::toString() const { return "FunCall"; }

// FunCall::check() required for CallExp inline function
std::shared_ptr<Type> FunCall::check(const Gamma&, const Delta&) const { 
    throw std::runtime_error("not needed");
}

// Stmts needs a non-inline check to generate vtable
bool Stmts::check(const Gamma&, const Delta&, const std::shared_ptr<Type>&, bool) const { return false; }

// Other statement types with inline print() - need check() for vtables
bool While::check(const Gamma&, const Delta&, const std::shared_ptr<Type>&, bool) const { return false; }
bool Assign::check(const Gamma&, const Delta&, const std::shared_ptr<Type>&, bool) const { return false; }
bool CallStmt::check(const Gamma&, const Delta&, const std::shared_ptr<Type>&, bool) const { return false; }
bool Return::check(const Gamma&, const Delta&, const std::shared_ptr<Type>&, bool) const { return false; }
bool Break::check(const Gamma&, const Delta&, const std::shared_ptr<Type>&, bool) const { return false; }
bool Continue::check(const Gamma&, const Delta&, const std::shared_ptr<Type>&, bool) const { return false; }

// Expression types with inline print/toString() - need check() for vtables
std::shared_ptr<Type> NilExp::check(const Gamma&, const Delta&) const { return nullptr; }
std::shared_ptr<Type> Select::check(const Gamma&, const Delta&) const { return nullptr; }
std::string Select::toString() const { return "Select"; }

} // namespace AST

// JSON to AST Conversion Implementations

namespace AST {

// Parses type representations from JSON
std::shared_ptr<Type> buildType(const nlohmann::json& j) {
    if (j.is_string()) {
        std::string typeStr = j.get<std::string>();
        if (typeStr == "Int") {
            return std::make_shared<IntType>();
        } else if (typeStr == "Nil") {
            return std::make_shared<NilType>();
        } else {
            throw std::runtime_error("Unknown type string: " + typeStr);
        }
    }
    
    if (j.is_object()) {
        if (j.contains("Ptr")) {
            auto pointeeType = buildType(j.at("Ptr"));
            return std::make_shared<PtrType>(pointeeType);
        } else if (j.contains("Array")) {
            auto elementType = buildType(j.at("Array"));
            return std::make_shared<ArrayType>(elementType);
        } else if (j.contains("Struct")) {
            std::string name = j.at("Struct");
            return std::make_shared<StructType>(name);
        } else if (j.contains("Fn")) {
            const auto& fnData = j.at("Fn");
            std::vector<std::shared_ptr<Type>> paramTypes;
            for (const auto& paramJson : fnData.at(0)) {
                paramTypes.push_back(buildType(paramJson));
            }
            auto returnType = buildType(fnData.at(1));
            return std::make_shared<FnType>(paramTypes, returnType);
        }
    }
    
    throw std::runtime_error("Unknown type format");
}

// Parses Place representations (Id, Deref, ArrayAccess, FieldAccess) from JSON.
std::unique_ptr<Place> buildPlace(const nlohmann::json& j) {
    if (j.contains("Id")) {
        std::string name = j.at("Id");
        return std::make_unique<Id>(name);
    } else if (j.contains("Deref")) {
        auto exp = buildExp(j.at("Deref"));
        return std::make_unique<Deref>(std::move(exp));
    } else if (j.contains("ArrayAccess")) {
        const auto& arr = j.at("ArrayAccess");
        // TS5 uses object format: {"array": exp, "idx": exp}
        auto array = buildExp(arr.at("array"));
        auto index = buildExp(arr.at("idx"));
        return std::make_unique<ArrayAccess>(std::move(array), std::move(index));
    } else if (j.contains("FieldAccess")) {
        const auto& fa = j.at("FieldAccess");
        auto ptr = buildExp(fa.at(0));
        std::string field = fa.at(1);
        return std::make_unique<FieldAccess>(std::move(ptr), field);
    } else {
        throw std::runtime_error("Unknown place format");
    }
}

// Parses Expression representations from JSON.
std::unique_ptr<Exp> buildExp(const nlohmann::json& j) {
    // TS4: Nil can be a bare string
    if (j.is_string() && j.get<std::string>() == "Nil") {
        return std::make_unique<NilExp>();
    }
    
    if (j.contains("Num")) {
        int value = j.at("Num");
        return std::make_unique<Num>(value);
    } else if (j.contains("Nil")) {
        return std::make_unique<NilExp>();
    } else if (j.contains("Val")) {
        auto place = buildPlace(j.at("Val"));
        return std::make_unique<Val>(std::move(place));
    } else if (j.contains("UnOp")) {
        const auto& unop = j.at("UnOp");
        std::string opStr;
        nlohmann::json expJson;
        
        // Handle both array format ["Neg", exp] and object format {"op": "Neg", "exp": exp}
        if (unop.is_array()) {
            opStr = unop.at(0);
            expJson = unop.at(1);
        } else {
            opStr = unop.at("op");
            expJson = unop.at("exp");
        }
        
        UnaryOp op;
        if (opStr == "Neg") {
            op = UnaryOp::Neg;
        } else if (opStr == "Not") {
            op = UnaryOp::Not;
        } else {
            throw std::runtime_error("Unknown unary operator: " + opStr);
        }
        auto exp = buildExp(expJson);
        return std::make_unique<UnOp>(op, std::move(exp));
    } else if (j.contains("BinOp")) {
        const auto& binop = j.at("BinOp");
        std::string opStr;
        nlohmann::json leftJson, rightJson;
        
        // Handle both array format ["Add", left, right] and object format {"op": "Add", "left": left, "right": right}
        if (binop.is_array()) {
            opStr = binop.at(0);
            leftJson = binop.at(1);
            rightJson = binop.at(2);
        } else {
            opStr = binop.at("op");
            leftJson = binop.at("left");
            rightJson = binop.at("right");
        }
        
        BinaryOp op;
        if (opStr == "Add") op = BinaryOp::Add;
        else if (opStr == "Sub") op = BinaryOp::Sub;
        else if (opStr == "Mul") op = BinaryOp::Mul;
        else if (opStr == "Div") op = BinaryOp::Div;
        else if (opStr == "Eq") op = BinaryOp::Eq;
        else if (opStr == "NotEq") op = BinaryOp::NotEq;
        else if (opStr == "Lt") op = BinaryOp::Lt;
        else if (opStr == "Lte") op = BinaryOp::Lte;
        else if (opStr == "Gt") op = BinaryOp::Gt;
        else if (opStr == "Gte") op = BinaryOp::Gte;
        else if (opStr == "And") op = BinaryOp::And;
        else if (opStr == "Or") op = BinaryOp::Or;
        else {
            throw std::runtime_error("Unknown binary operator: " + opStr);
        }
        auto left = buildExp(leftJson);
        auto right = buildExp(rightJson);
        return std::make_unique<BinOp>(op, std::move(left), std::move(right));
    } else if (j.contains("Select")) {
        const auto& sel = j.at("Select");
        // TS3 uses object format: {"guard": exp, "tt": exp, "ff": exp}
        auto guard = buildExp(sel.at("guard"));
        auto trueBranch = buildExp(sel.at("tt"));
        auto falseBranch = buildExp(sel.at("ff"));
        return std::make_unique<Select>(std::move(guard), std::move(trueBranch), std::move(falseBranch));
    } else if (j.contains("Call")) {
        auto funcall = buildFunCall(j.at("Call"));
        return std::make_unique<CallExp>(std::move(funcall));
    } else if (j.contains("NewArray")) {
        const auto& na = j.at("NewArray");
        auto type = buildType(na.at(0));
        auto amount = buildExp(na.at(1));
        return std::make_unique<NewArray>(type, std::move(amount));
    } else if (j.contains("NewSingle")) {
        auto type = buildType(j.at("NewSingle"));
        return std::make_unique<NewSingle>(type);
    } else {
        std::string keys;
        std::string typeinfo = "unknown";
        if (j.is_object()) {
            typeinfo = "object";
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (!keys.empty()) keys += ", ";
                keys += it.key();
            }
        } else if (j.is_array()) {
            typeinfo = "array of size " + std::to_string(j.size());
        } else if (j.is_string()) {
            typeinfo = "string: " + j.get<std::string>();
        } else if (j.is_number()) {
            typeinfo = "number";
        } else if (j.is_null()) {
            typeinfo = "null";
        } else if (j.is_boolean()) {
            typeinfo = "boolean";
        }
        throw std::runtime_error("Unknown expression format. Type: " + typeinfo + ", Keys: " + keys);
    }
}

// Parses FunCall representation from JSON.
std::unique_ptr<FunCall> buildFunCall(const nlohmann::json& j) {
    auto callee = buildExp(j.at(0));
    std::vector<std::unique_ptr<Exp>> args;
    for (const auto& argJson : j.at(1)) {
        args.push_back(buildExp(argJson));
    }
    return std::make_unique<FunCall>(std::move(callee), std::move(args));
}

// Parses Statement representations from JSON.
std::unique_ptr<Stmt> buildStmt(const nlohmann::json& j) {
    // TS3: Break and Continue can be bare strings in statement arrays
    if (j.is_string()) {
        std::string s = j;
        if (s == "Break") {
            return std::make_unique<Break>();
        } else if (s == "Continue") {
            return std::make_unique<Continue>();
        } else {
            throw std::runtime_error("Unknown statement string: " + s);
        }
    }
    
    if (j.contains("Assign")) {
        const auto& assign = j.at("Assign");
        auto lhs = buildPlace(assign.at(0));
        auto rhs = buildExp(assign.at(1));
        return std::make_unique<Assign>(std::move(lhs), std::move(rhs));
    } else if (j.contains("Call")) {
        auto funcall = buildFunCall(j.at("Call"));
        return std::make_unique<CallStmt>(std::move(funcall));
    } else if (j.contains("If")) {
        const auto& ifstmt = j.at("If");
        // TS3 uses object format: {"guard": exp, "tt": [stmts], "ff": [stmts]}
        auto guard = buildExp(ifstmt.at("guard"));
        
        // tt and ff are arrays of statements
        auto trueBranchStmts = std::make_unique<Stmts>();
        for (const auto& stmtJson : ifstmt.at("tt")) {
            trueBranchStmts->statements.push_back(buildStmt(stmtJson));
        }
        
        std::optional<std::unique_ptr<Stmt>> falseBranch;
        if (ifstmt.contains("ff") && ifstmt.at("ff").is_array() && !ifstmt.at("ff").empty()) {
            auto falseBranchStmts = std::make_unique<Stmts>();
            for (const auto& stmtJson : ifstmt.at("ff")) {
                falseBranchStmts->statements.push_back(buildStmt(stmtJson));
            }
            falseBranch = std::move(falseBranchStmts);
        }
        
        return std::make_unique<If>(std::move(guard), std::move(trueBranchStmts), std::move(falseBranch));
    } else if (j.contains("While")) {
        const auto& whilestmt = j.at("While");
        // TS3 uses array format: [guard, [stmts]]
        auto guard = buildExp(whilestmt.at(0));
        
        // Body is an array of statements
        auto bodyStmts = std::make_unique<Stmts>();
        for (const auto& stmtJson : whilestmt.at(1)) {
            bodyStmts->statements.push_back(buildStmt(stmtJson));
        }
        
        return std::make_unique<While>(std::move(guard), std::move(bodyStmts));
    } else if (j.contains("Return")) {
        std::optional<std::unique_ptr<Exp>> exp;
        if (!j.at("Return").is_null()) {
            exp = buildExp(j.at("Return"));
        }
        return std::make_unique<Return>(std::move(exp));
    } else if (j.contains("Break")) {
        return std::make_unique<Break>();
    } else if (j.contains("Continue")) {
        return std::make_unique<Continue>();
    } else if (j.contains("Stmts")) {
        auto result = std::make_unique<Stmts>();
        for (const auto& stmtJson : j.at("Stmts")) {
            result->statements.push_back(buildStmt(stmtJson));
        }
        return result;
    } else {
        throw std::runtime_error("Unknown statement format");
    }
}

// Parses Decl representations (used in params, locals, fields) from JSON.
Decl buildDecl(const nlohmann::json& j) {
    std::string name = j.at("name");
    auto type = buildType(j.at("typ"));
    return Decl(name, type);
}

// Parses FunctionDef representations from JSON.
std::unique_ptr<FunctionDef> buildFunctionDef(const nlohmann::json& j) {
    auto result = std::make_unique<FunctionDef>();
    result->name = j.at("name");
    
    for (const auto& paramJson : j.at("prms")) {
        result->params.push_back(buildDecl(paramJson));
    }
    
    result->rettype = buildType(j.at("rettyp"));
    
    for (const auto& localJson : j.at("locals")) {
        result->locals.push_back(buildDecl(localJson));
    }
    
    // Function body is a Stmts node containing the statement list
    auto stmts_node = std::make_unique<Stmts>();
    for (const auto& stmtJson : j.at("stmts")) {
        stmts_node->statements.push_back(buildStmt(stmtJson));
    }
    result->body = std::move(stmts_node);
    
    return result;
}

// Parses StructDef representations from JSON.
std::unique_ptr<StructDef> buildStructDef(const nlohmann::json& j) {
    auto result = std::make_unique<StructDef>();
    result->name = j.at("name");
    
    for (const auto& fieldJson : j.at("fields")) {
        result->fields.push_back(buildDecl(fieldJson));
    }
    
    return result;
}

// Parses Extern representations from JSON.
Extern buildExtern(const nlohmann::json& j) {
    std::string name = j.at("name");
    
    std::vector<std::shared_ptr<Type>> paramTypes;
    for (const auto& paramJson : j.at("prms")) {
        paramTypes.push_back(buildType(paramJson));
    }
    
    auto returnType = buildType(j.at("rettyp"));
    
    Extern ext;
    ext.name = name;
    ext.param_types = paramTypes;
    ext.rettype = returnType;
    return ext;
}

} // namespace AST

// buildProgram is in global namespace
std::unique_ptr<AST::Program> buildProgram(const nlohmann::json& j) {
    auto result = std::make_unique<AST::Program>();
    
    for (const auto& structJson : j.at("structs")) {
        result->structs.push_back(AST::buildStructDef(structJson));
    }
    
    for (const auto& externJson : j.at("externs")) {
        result->externs.push_back(AST::buildExtern(externJson));
    }
    
    for (const auto& funcJson : j.at("functions")) {
        result->functions.push_back(AST::buildFunctionDef(funcJson));
    }
    
    return result;
}

// Global namespace forward declaration implementation
std::unique_ptr<AST::FunCall> buildFunCall(const nlohmann::json& j) {
    return AST::buildFunCall(j);
}

// Global namespace buildExp wrapper
std::unique_ptr<AST::Exp> buildExp(const nlohmann::json& j) {
    return AST::buildExp(j);
}
