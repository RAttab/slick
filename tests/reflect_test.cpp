/* reflect_test.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 08 Mar 2014
   FreeBSD-style copyright and disclaimer apply

   Reflection tests
*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

using namespace std;

namespace blah {

struct Foo
{
    int a;
    void connect(int b)
    {
        a = b;
    }
};

} // namespace blah

REFLECT_CLASS(Foo, blah)
{
    REFLECT_FIELD(a, Foo::a);

    REFLECT_FIELD(connect, Foo::connect);

    REFLECT_FUNCTION_CUSTOM(blah) (int first, int second) -> int
    {
        return a = first + second;
    };
};



BOOST_AUTO_TEST_CASE()
{
    Reflection* foo = ReflectionRegistry::get<Foo>();
    Value val = foo->construct();
    val.set("a", 10);
    val.set("connect");
}
