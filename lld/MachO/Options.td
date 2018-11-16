include "llvm/Option/OptParser.td"

def arch: Separate<["-"], "arch">, MetaVarName<"<arch-name>">,
     HelpText<"Architecture to link">;

def e: Separate<["-"], "e">, HelpText<"Name of entry point symbol">;

def o: Separate<["-"], "o">, MetaVarName<"<path>">,
  HelpText<"Path to file to write output">;
