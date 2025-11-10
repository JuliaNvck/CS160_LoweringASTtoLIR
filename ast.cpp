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

// Stub implementations for virtual functions declared in ast.hpp
// These are not needed for the lowerer, but must exist for the vtable

bool StructType::equals(const Type& other) const {
    const StructType* o = dynamic_cast<const StructType*>(&other);
    return o && (name == o->name);
}

bool ArrayType::equals(const Type& other) const {
    const ArrayType* o = dynamic_cast<const ArrayType*>(&other);
    return o && elementType->equals(*o->elementType);
}

bool PtrType::equals(const Type& other) const {
    const PtrType* o = dynamic_cast<const PtrType*>(&other);
    return o && pointeeType->equals(*o->pointeeType);
}

bool FnType::equals(const Type& other) const {
    const FnType* o = dynamic_cast<const FnType*>(&other);
    if (!o || paramTypes.size() != o->paramTypes.size()) return false;
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (!paramTypes[i]->equals(*o->paramTypes[i])) return false;
    }
    return returnType->equals(*o->returnType);
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
std::string ArrayAccess::toString() const { return array->toString() + "[" + index->toString() + "]"; }
std::string FieldAccess::toString() const { return ptr->toString() + "." + field; }

// Provide minimal stubs for other functions that may be referenced
// These are declared in ast.hpp but you'll implement them when needed

std::shared_ptr<Type> buildType(const nlohmann::json& j) {
    // Stub - implement when needed
    (void)j;
    throw std::runtime_error("buildType not implemented - not needed for lowerer");
}

std::unique_ptr<Exp> buildExp(const nlohmann::json& j) {
    (void)j;
    throw std::runtime_error("buildExp not implemented - not needed for lowerer");
}

std::unique_ptr<Place> buildPlace(const nlohmann::json& j) {
    (void)j;
    throw std::runtime_error("buildPlace not implemented - not needed for lowerer");
}

std::unique_ptr<Stmt> buildStmt(const nlohmann::json& j) {
    (void)j;
    throw std::runtime_error("buildStmt not implemented - not needed for lowerer");
}

Decl buildDecl(const nlohmann::json& j) {
    (void)j;
    throw std::runtime_error("buildDecl not implemented - not needed for lowerer");
}

std::unique_ptr<FunctionDef> buildFunctionDef(const nlohmann::json& j) {
    (void)j;
    throw std::runtime_error("buildFunctionDef not implemented - not needed for lowerer");
}

std::unique_ptr<StructDef> buildStructDef(const nlohmann::json& j) {
    (void)j;
    throw std::runtime_error("buildStructDef not implemented - not needed for lowerer");
}

Extern buildExtern(const nlohmann::json& j) {
    (void)j;
    throw std::runtime_error("buildExtern not implemented - not needed for lowerer");
}

} // namespace AST

// buildProgram is in global namespace
std::unique_ptr<AST::Program> buildProgram(const nlohmann::json& j) {
    (void)j;
    throw std::runtime_error("buildProgram not implemented yet - you need to implement this to parse .astj files");
}

std::unique_ptr<AST::FunCall> buildFunCall(const nlohmann::json& j) {
    (void)j;
    throw std::runtime_error("buildFunCall not implemented - not needed for lowerer");
}

AST::Gamma construct_gamma(const std::vector<AST::Extern>& externs, const std::vector<std::unique_ptr<AST::FunctionDef>>& functions) {
    (void)externs;
    (void)functions;
    throw std::runtime_error("construct_gamma not implemented - not needed for lowerer");
}

AST::Delta construct_delta(const std::vector<std::unique_ptr<AST::StructDef>>& structs) {
    (void)structs;
    throw std::runtime_error("construct_delta not implemented - not needed for lowerer");
}
