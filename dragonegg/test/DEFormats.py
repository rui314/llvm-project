import DETestRunner
from TestFormats import FileBasedTest

class CompilatorTest(FileBasedTest):
    def __init__(self, compilers, compiler_flags, skip, xfails):
        self.compilers = compilers
        self.compiler_flags = compiler_flags
        self.skip = skip
        self.xfails = xfails

    def execute(self, test, litConfig):
        return DETestRunner.executeCompilatorTest(test, litConfig,
          self.compilers, self.compiler_flags, self.skip, self.xfails)
