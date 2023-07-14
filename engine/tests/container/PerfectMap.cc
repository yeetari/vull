#include <vull/container/PerfectMap.hh>

#include <vull/container/MapEntry.hh>
#include <vull/container/Vector.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Test.hh>
#include <vull/support/Utility.hh>

#include <stddef.h>

using namespace vull;

TEST_CASE(PerfectMap, FromEntries) {
    Vector<MapEntry<String, size_t>> entries;
    entries.push({"Foo", 5});
    entries.push({"Bar", 7});
    entries.push({"Baz", 11});

    auto map = PerfectMap<String, size_t>::from_entries(entries);
    EXPECT(map.contains("Foo"));
    EXPECT(map.contains("Bar"));
    EXPECT(map.contains("Baz"));
    EXPECT(!map.contains("FooBar"));

    EXPECT(!map.get("FooBar"));
    EXPECT(map.get("Foo"));
    EXPECT(*map.get("Bar") == 7);
}
