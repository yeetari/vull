#include <vull/support/Enum.hh>

#include <vull/support/StringView.hh>

namespace foo::bar {

enum class MyEnum {
    Foo,
    Bar,
};

static_assert(vull::enum_name_long<MyEnum, MyEnum::Foo>() == "foo::bar::MyEnum::Foo");
static_assert(vull::enum_name_long<MyEnum, MyEnum::Bar>() == "foo::bar::MyEnum::Bar");
static_assert(vull::enum_name<MyEnum, MyEnum::Foo, 1>() == "Foo");
static_assert(vull::enum_name<MyEnum, MyEnum::Bar, 1>() == "Bar");
static_assert(vull::enum_name<MyEnum, MyEnum::Foo, 2>() == "MyEnum::Foo");
static_assert(vull::enum_name<MyEnum, MyEnum::Bar, 2>() == "MyEnum::Bar");
static_assert(vull::enum_name<MyEnum, MyEnum::Foo, 3>() == "bar::MyEnum::Foo");
static_assert(vull::enum_name<MyEnum, MyEnum::Bar, 3>() == "bar::MyEnum::Bar");
static_assert(vull::enum_name(MyEnum::Foo) == "MyEnum::Foo");
static_assert(vull::enum_name(MyEnum::Bar) == "MyEnum::Bar");
static_assert(vull::enum_name<3>(MyEnum::Foo) == "bar::MyEnum::Foo");
static_assert(vull::enum_name<3>(MyEnum::Bar) == "bar::MyEnum::Bar");

} // namespace foo::bar
