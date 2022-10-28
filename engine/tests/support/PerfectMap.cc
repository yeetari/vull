#include <vull/support/PerfectMap.hh>

#include <vull/support/Integral.hh>
#include <vull/support/MapEntry.hh>
#include <vull/support/Optional.hh>
#include <vull/support/PerfectHasher.hh>
#include <vull/support/String.hh>
#include <vull/support/Test.hh>
#include <vull/support/Vector.hh>

#include <stddef.h>

using namespace vull;

TEST_SUITE(PerfectMap, {
    ;
    TEST_CASE(FromEntries) {
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
})