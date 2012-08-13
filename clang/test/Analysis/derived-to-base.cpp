// RUN: %clang_cc1 -analyze -analyzer-checker=core,debug.ExprInspection -analyzer-store=region -verify %s

void clang_analyzer_eval(bool);

class A {
protected:
  int x;
};

class B : public A {
public:
  void f();
};

void B::f() {
  x = 3;
}


class C : public B {
public:
  void g() {
    // This used to crash because we are upcasting through two bases.
    x = 5;
  }
};


namespace VirtualBaseClasses {
  class A {
  protected:
    int x;
  };

  class B : public virtual A {
  public:
    int getX() { return x; }
  };

  class C : public virtual A {
  public:
    void setX() { x = 42; }
  };

  class D : public B, public C {};
  class DV : virtual public B, public C {};
  class DV2 : public B, virtual public C {};

  void test() {
    D d;
    d.setX();
    clang_analyzer_eval(d.getX() == 42); // expected-warning{{TRUE}}

    DV dv;
    dv.setX();
    clang_analyzer_eval(dv.getX() == 42); // expected-warning{{TRUE}}

    DV2 dv2;
    dv2.setX();
    clang_analyzer_eval(dv2.getX() == 42); // expected-warning{{TRUE}}
  }


  // Make sure we're consistent about the offset of the A subobject within an
  // Intermediate virtual base class.
  class Padding1 { int unused; };
  class Padding2 { int unused; };
  class Intermediate : public Padding1, public A, public Padding2 {};

  class BI : public virtual Intermediate {
  public:
    int getX() { return x; }
  };

  class CI : public virtual Intermediate {
  public:
    void setX() { x = 42; }
  };

  class DI : public BI, public CI {};

  void testIntermediate() {
    DI d;
    d.setX();
    clang_analyzer_eval(d.getX() == 42); // expected-warning{{TRUE}}
  }
}
