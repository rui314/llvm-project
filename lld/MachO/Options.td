include "llvm/Option/OptParser.td"

def e: Separate<["-"], "e">, HelpText<"Name of entry point symbol">;

def o: Separate<["-"], "o">, MetaVarName<"<path>">,
  HelpText<"Path to file to write output">;
