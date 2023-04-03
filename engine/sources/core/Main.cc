#include <vull/core/Main.hh>
#include <vull/support/Assert.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/Scheduler.hh>

using namespace vull;

int main(int argc, char **argv) {
    Scheduler scheduler;
    scheduler.start([=] {
        Vector<StringView> args(argv, argv + argc);
        vull_main(vull::move(args));
    });
}
