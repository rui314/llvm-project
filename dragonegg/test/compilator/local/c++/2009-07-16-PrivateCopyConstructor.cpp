#include <set>

class A {
public:
  A();
private:
  A(const A&);
};
void B()
{
  std::set<void *, A> foo;
}
