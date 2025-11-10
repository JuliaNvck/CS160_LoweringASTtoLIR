#include <iostream>
#include <fstream>
#include <memory>

#include "json.hpp"     // Your JSON library
#include "ast.hpp"      // Your AST header
#include "lowerer.hpp"    // Our new lowerer

// This function must be defined in your ast.cpp
std::unique_ptr<AST::Program> buildProgram(const nlohmann::json& j);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file.astj>\n";
        return 1;
    }
    
    // 1. Open and read the input file
    std::ifstream input_file(argv[1]);
    if (!input_file.is_open()) {
        std::cerr << "Error: Could not open file " << argv[1] << "\n";
        return 1;
    }
    
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(input_file);
    } catch (nlohmann::json::parse_error& e) {
        std::cerr << "Error: Failed to parse JSON.\n" << e.what() << std::endl;
        return 1;
    }

    // 2. Parse the AST (using your ast.cpp function)
    std::unique_ptr<AST::Program> ast_prog;
    try {
        ast_prog = buildProgram(j);
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to build AST from JSON.\n" << e.what() << std::endl;
        return 1;
    }
    
    // 3. Lower the AST to LIR
    std::unique_ptr<LIR::Program> lir_prog;
    try {
        Lowerer lowerer;
        lir_prog = lowerer.lower(ast_prog.get());
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed during lowering.\n" << e.what() << std::endl;
        return 1;
    }
    
    // 4. Print the LIR program to standard out
    // This uses the operator<< from lir.h
    std::cout << *lir_prog;
    
    return 0;
}