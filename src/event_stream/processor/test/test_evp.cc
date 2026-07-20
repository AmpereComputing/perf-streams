// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/processor/bounds.h"
#include "event_stream/testing/build_event_stream.h"
#include "testing/run_tool.h"

#include <cstdio>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define EVP_PROGRAM BINDIR "/../evp"

#define EXPECT_PLUGIN_ERROR(output, plugin, message) \
    EXPECT_EQ(output, "Error: issue constructing plugin \"" plugin "\"\n  " message)

using std::filesystem::path;

class EVPTest : public ::testing::Test
{
protected:
    std::string run(const std::string& cmd, int expected_status = 0)
    {
        auto [output, status] = run_with_status(cmd);
        EXPECT_EQ(status, expected_status);
        return output;
    }

    std::string run_expecting_error(const std::string& cmd) { return run(cmd, 256); }

    std::pair<std::string, int> run_with_status(const std::string& cmd)
    {
        return perf_streams::testing::run_tool(EVP_PROGRAM, cmd);
    }

    static std::string stream(const std::string& name) { return (path(TEST_DIRECTORY) / "streams" / name).string(); }
    static std::string stream_output(const std::string& name) { return (path(TEST_OUTPUT_DIRECTORY) / name).string(); }
    static std::string config(const std::string& name) { return (path(TEST_DIRECTORY) / "configs" / name).string(); }

    std::string test_file(const std::string& name, const std::string& prefix = "") const
    {
        const auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
        return fmt::format("{}{}_{}_{}", prefix, test_info->test_case_name(), test_info->name(), name);
    }

    static std::string read_and_remove_file(const std::string& filename)
    {
        std::ifstream const inf{filename};
        std::stringstream ss;
        ss << inf.rdbuf();

        std::remove(filename.c_str());
        return ss.str();
    }

    static std::string build_es(const std::string& name)
    {
        auto output_name = name.substr(0, name.rfind('.')) + ".es";
        return perf_streams::event_stream::testing::build_es(stream(name), stream_output(output_name));
    }

    static std::string build_es_xz(const std::string& name)
    {
        auto output_name = name.substr(0, name.rfind('.')) + ".es.xz";
        return perf_streams::event_stream::testing::build_es(stream(name), stream_output(output_name));
    }
};

TEST_F(EVPTest, CanCountAllEvents)
{
    auto output = run(fmt::format("--es {} +count -a +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "one 2\ntwo 2\n");
}

TEST_F(EVPTest, CanCountOneEvent)
{
    auto output = run(fmt::format("--es {} +count -e two +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "two 2\n");
}

TEST_F(EVPTest, CanCountPatterns)
{
    auto patterns_es = build_es("patterns.in");

    auto output = run(fmt::format("--es {} +count -e 'a_0.*.foo' +summarize", patterns_es));
    const auto* expected = R"(a_0.b_0.foo 1
a_0.b_1.foo 1
a_0.b_2.foo 1
a_0.b_3.foo 1
)";
    EXPECT_EQ(output, expected);

    output = run(fmt::format("--es {} +count -e 'a_1.*.foo' +summarize", patterns_es));
    EXPECT_EQ(output, "a_1.b_0.foo 1\na_1.b_1.foo 2\n");

    output = run(fmt::format("--es {} +count -e '*.b_1.foo' +summarize", patterns_es));
    EXPECT_EQ(output, "a_0.b_1.foo 1\na_1.b_1.foo 2\n");

    output = run(fmt::format("--es {} +count -e '*.b_3.foo' +summarize", patterns_es));
    EXPECT_EQ(output, "a_0.b_3.foo 1\n");

    output = run(fmt::format("--es {} +count -e 'a_#.b_1.*' +summarize", patterns_es));
    EXPECT_EQ(output, "a_0.b_1.foo 1\na_1.b_1.foo 2\n");

    output = run(fmt::format("--es {} +count -e '*b_0.f*' +summarize", patterns_es));
    EXPECT_EQ(output, "a_0.b_0.foo 1\na_1.b_0.foo 1\n");

    output = run(fmt::format("--es {} +count -e 'a_0.b_?.f*' +summarize", patterns_es));
    EXPECT_EQ(output, "a_0.b_0.foo 1\na_0.b_1.foo 1\na_0.b_2.foo 1\na_0.b_3.foo 1\n");

    output = run(fmt::format("--es {} +count -e 'a_0.b_[02].f*' +summarize", patterns_es));
    EXPECT_EQ(output, "a_0.b_0.foo 1\na_0.b_2.foo 1\n");

    output = run(fmt::format("--es {} +count -e 'a_0.b_[!02].f*' +summarize", patterns_es));
    EXPECT_EQ(output, "a_0.b_1.foo 1\na_0.b_3.foo 1\n");
}

TEST_F(EVPTest, CanExcludeCountingEvents)
{
    auto output = run(fmt::format("--es {} +count -e one -e two -x one +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "two 2\n");

    output = run(fmt::format("--es {} +count -e one -e two -x one -e one -x two +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "one 2\n");
}

TEST_F(EVPTest, CanExcludeCountingPatterns)
{
    auto patterns_es = build_es("patterns.in");

    auto output = run(fmt::format("--es {} +count -e 'a_0.*.foo' -x 'a_0.b_[13].foo' +summarize", patterns_es));
    const auto* expected = R"(a_0.b_0.foo 1
a_0.b_2.foo 1
)";
    EXPECT_EQ(output, expected);

    output =
        run(fmt::format("--es {} +count -e 'a_0.*.foo' -x 'a_0.b_*.foo' -e a_0.b_[02].foo +summarize", patterns_es));
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CountOfNonexistantEventIsAnError)
{
    auto output = run_expecting_error(fmt::format("--es {} +count -e blah +summarize", build_es("basic.in")));
    EXPECT_PLUGIN_ERROR(output, "count", "no definition for event or data item \"blah\"\n");
}

TEST_F(EVPTest, MultipleCount)
{
    auto output =
        run(fmt::format("--es {} +count -e 'a_0.b_1.foo' +summarize +count -e 'a_1.b_0.foo'", build_es("patterns.in")));
    EXPECT_EQ(output, "a_0.b_1.foo 1\na_1.b_0.foo 1\n");
}

TEST_F(EVPTest, BoundsParseFromSpec)
{
    auto f = perf_streams::event_stream::processor::FactorBounds::from_spec("[0:40:23]");
    EXPECT_EQ(f.minimum, 0);
    EXPECT_EQ(f.maximum, 40);
    EXPECT_EQ(f.granularity, 23);
    EXPECT_EQ(f.sequence, perf_streams::event_stream::processor::FactorBounds::LINEAR);

    f = perf_streams::event_stream::processor::FactorBounds::from_spec("[-13:923]");
    EXPECT_EQ(f.minimum, -13);
    EXPECT_EQ(f.maximum, 923);
    EXPECT_EQ(f.granularity, 1);
    EXPECT_EQ(f.sequence, perf_streams::event_stream::processor::FactorBounds::LINEAR);

    f = perf_streams::event_stream::processor::FactorBounds::from_spec("[1:]");
    EXPECT_EQ(f.minimum, 1);
    EXPECT_EQ(f.maximum, std::nullopt);
    EXPECT_EQ(f.granularity, 1);
    EXPECT_EQ(f.sequence, perf_streams::event_stream::processor::FactorBounds::LINEAR);

    f = perf_streams::event_stream::processor::FactorBounds::from_spec("[10::]");
    EXPECT_EQ(f.minimum, 10);
    EXPECT_EQ(f.maximum, std::nullopt);
    EXPECT_EQ(f.granularity, 1);
    EXPECT_EQ(f.sequence, perf_streams::event_stream::processor::FactorBounds::LINEAR);

    f = perf_streams::event_stream::processor::FactorBounds::from_spec("[:10:]");
    EXPECT_EQ(f.minimum, std::nullopt);
    EXPECT_EQ(f.maximum, 10);
    EXPECT_EQ(f.granularity, 1);
    EXPECT_EQ(f.sequence, perf_streams::event_stream::processor::FactorBounds::LINEAR);

    f = perf_streams::event_stream::processor::FactorBounds::from_spec("[::10]");
    EXPECT_EQ(f.minimum, std::nullopt);
    EXPECT_EQ(f.maximum, std::nullopt);
    EXPECT_EQ(f.granularity, 10);
    EXPECT_EQ(f.sequence, perf_streams::event_stream::processor::FactorBounds::LINEAR);

    f = perf_streams::event_stream::processor::FactorBounds::from_spec("[::2:exp]");
    EXPECT_EQ(f.minimum, std::nullopt);
    EXPECT_EQ(f.maximum, std::nullopt);
    EXPECT_EQ(f.granularity, 2);
    EXPECT_EQ(f.sequence, perf_streams::event_stream::processor::FactorBounds::EXPONENTIAL);

    f = perf_streams::event_stream::processor::FactorBounds::from_spec("[2:16:2:exp]");
    EXPECT_EQ(f.minimum, 2);
    EXPECT_EQ(f.maximum, 16);
    EXPECT_EQ(f.granularity, 2);
    EXPECT_EQ(f.sequence, perf_streams::event_stream::processor::FactorBounds::EXPONENTIAL);

    f = perf_streams::event_stream::processor::FactorBounds::from_spec("[:10::linear]");
    EXPECT_EQ(f.minimum, std::nullopt);
    EXPECT_EQ(f.maximum, 10);
    EXPECT_EQ(f.granularity, 1);
    EXPECT_EQ(f.sequence, perf_streams::event_stream::processor::FactorBounds::LINEAR);

    f = perf_streams::event_stream::processor::FactorBounds::from_spec("[1:10::linear]");
    EXPECT_EQ(f.minimum, 1);
    EXPECT_EQ(f.maximum, 10);
    EXPECT_EQ(f.granularity, 1);
    EXPECT_EQ(f.sequence, perf_streams::event_stream::processor::FactorBounds::LINEAR);
}

TEST_F(EVPTest, CountByFactors)
{
    auto output = run(fmt::format("--es {} +count -e ev/a +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev     10
ev/a:1 1
ev/a:2 2
ev/a:3 3
ev/a:4 4
)");
}

TEST_F(EVPTest, CountByFactorValue)
{
    auto output = run(fmt::format("--es {} +count -e ev/a:2 +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev/a:2 2
)");
}

TEST_F(EVPTest, CountByMixedFactorValue)
{
    auto output = run(fmt::format("--es {} +count -e ev/a:2/b +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev/a:2/b:1 2
)");
}

TEST_F(EVPTest, CountByMultipleFactorValues)
{
    auto output = run(fmt::format("--es {} +count -e ev/a:3/b:2 +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev/a:3/b:2 1
)");
}

TEST_F(EVPTest, CountByEnumFactorValue)
{
    auto output = run(fmt::format("--es {} +count -e ev/myenum:b +summarize", build_es("enums.in")));
    EXPECT_EQ(output, R"(ev/myenum:b 1
)");

    output = run(fmt::format("--es {} +count -e ev/myenum:2 +summarize", build_es("enums.in")));
    EXPECT_EQ(output, R"(ev/myenum:b 1
)");
}

TEST_F(EVPTest, CountByFactorValueRemovesDistinctCounters)
{
    auto output = run(fmt::format("--es {} +count -e ev/a:2 -x ev/a:3 +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev/a:2 2
)");
}

TEST_F(EVPTest, CountByFactorsWithMalformedBucketsRaiseError)
{
    EXPECT_NE(run_with_status(fmt::format("--es {} +count -e ev/a[] +summarize", build_es("factors.in"))).second, 0);
    EXPECT_NE(run_with_status(fmt::format("--es {} +count -e ev/a[: +summarize", build_es("factors.in"))).second, 0);
    EXPECT_NE(run_with_status(fmt::format("--es {} +count -e ev/a[:] +summarize", build_es("factors.in"))).second, 0);
}

TEST_F(EVPTest, CountByFactorsWithBucketMiniumnMaximum)
{
    auto output = run(fmt::format("--es {} +count -e ev/a[2:3] +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev     10
ev/a:2 3
ev/a:3 7
)");
}

TEST_F(EVPTest, CountByFactorsWithBucketMinimumGranularity)
{
    auto output = run(fmt::format("--es {} +count -e ev/a[1::2] +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev     10
ev/a:1 1
ev/a:2 5
ev/a:4 4
)");
}

TEST_F(EVPTest, CountByFactorsWithBucketMaximumGranularity)
{
    auto output = run(fmt::format("--es {} +count -e ev/a[:3:2] +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev     10
ev/a:0 1
ev/a:2 5
ev/a:3 4
)");
}

TEST_F(EVPTest, CountByFactorsWithGranularityOnly)
{
    auto output = run(fmt::format("--es {} +count -e ev/a[::2] +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev     10
ev/a:0 1
ev/a:2 5
ev/a:4 4
)");
}

TEST_F(EVPTest, CountByFactorsWithBucketAll)
{
    auto output = run(fmt::format("--es {} +count -e ev/a[0:4:2] +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev     10
ev/a:0 1
ev/a:2 5
ev/a:4 4
)");
}

TEST_F(EVPTest, CountByFactorsWithBucketExponentialBase2)
{
    auto output = run(fmt::format("--es {} +count -e ev/a[::2:exp] +summarize", build_es("larger_factors.in")));
    EXPECT_EQ(output, R"(ev      13
ev/a:1  1
ev/a:2  4
ev/a:4  5
ev/a:8  2
ev/a:16 1
)");
}

TEST_F(EVPTest, CountByFactorsWithBucketExponentialBase10)
{
    auto output = run(fmt::format("--es {} +count -e ev/a[::10:exp] +summarize", build_es("larger_factors.in")));
    EXPECT_EQ(output, R"(ev      13
ev/a:1  10
ev/a:10 3
)");
}

TEST_F(EVPTest, CountByFactorsWithBucketExponential)
{
    auto output = run(fmt::format("--es {} +count -e ev/a[::3:exp] +summarize", build_es("larger_factors.in")));
    EXPECT_EQ(output, R"(ev     13
ev/a:1 3
ev/a:3 7
ev/a:9 3
)");
}

TEST_F(EVPTest, CountByTwoFactors)
{
    auto output = run(fmt::format("--es {} +count -e ev/a/b +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev         10
ev/a:1     1
ev/a:2/b:1 2
ev/a:3     2
ev/a:3/b:2 1
ev/a:4     3
ev/a:4/b:2 1
)");
}

TEST_F(EVPTest, ScopedFactorComesBeforeGlobalFactor)
{
    auto output = run(fmt::format("--es {} +count -e mod.ev/a +summarize", build_es("factors_scoped.in")));
    EXPECT_EQ(output, R"(mod.ev         2
mod.ev/mod.a:0 1
mod.ev/mod.a:1 1
)");
}

TEST_F(EVPTest, ForceGlobalFactor)
{
    auto output = run(fmt::format("--es {} +count -e mod.ev2/.a +summarize", build_es("factors_scoped.in")));
    EXPECT_EQ(output, R"(mod.ev2     1
mod.ev2/a:2 1
)");
}

TEST_F(EVPTest, CanAccumulateData)
{
    auto output =
        run(fmt::format("--es {} +count --accumulate ev/a --accumulate ev/b +summarize", build_es("factors.in")));
    EXPECT_EQ(output, R"(ev/a 30
ev/b 6
)");
}

TEST_F(EVPTest, CantAccumulateWithMoreThanOneDataItem)
{
    auto output =
        run_expecting_error(fmt::format("--es {} +count --accumulate ev/a/b +summarize", build_es("factors.in")));
    EXPECT_PLUGIN_ERROR(output, "count", "malformed data counter; should have exactly one data item: <event>/<data>\n");
}

TEST_F(EVPTest, CantAccumulateDataItemWithValue)
{
    auto output =
        run_expecting_error(fmt::format("--es {} +count --accumulate ev/a:2 +summarize", build_es("factors.in")));
    EXPECT_PLUGIN_ERROR(output, "count", "factor \"a\" is not allowed a value in \"a/ev\"\n");
}

TEST_F(EVPTest, CanRename)
{
    auto output = run(fmt::format("--es {} +count -e one +rename -r one=One +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "One 2\n");
}

TEST_F(EVPTest, CanCopy)
{
    auto output = run(fmt::format("--es {} +count -e one +rename -c one=One +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "One 2\none 2\n");
}

TEST_F(EVPTest, CanRenameToLowercase)
{
    auto output = run(fmt::format("--es {} +count -e ev/myenum +rename -rl '(*)/myenum:(*)=$1.$2' +summarize",
                                  build_es("uppercase_enums.in")));
    const auto* expected = R"(ev           2
ev.blue_fish 1
ev.red_fish  1
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanCopyToLowercase)
{
    auto output = run(fmt::format("--es {} +count -e ev/myenum +rename -cl '(*)/myenum:(*)=$1.$2' +summarize",
                                  build_es("uppercase_enums.in")));
    const auto* expected = R"(ev                  2
ev.blue_fish        1
ev.red_fish         1
ev/myenum:BLUE_FISH 1
ev/myenum:RED_FISH  1
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanCopyRenamed)
{
    auto output =
        run(fmt::format("--es {} +count -e one +rename -r one=One -c One=Two +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "One 2\nTwo 2\n");
}

TEST_F(EVPTest, RenameCopyMaintainOrdering)
{
    auto output =
        run(fmt::format("--es {} +count -e one +rename -r one=One -c One=Two -r Two=two -c two=Three +summarize",
                        build_es("basic.in")));
    EXPECT_EQ(output, "One   2\nThree 2\ntwo   2\n");
}

TEST_F(EVPTest, CanRenamePattern)
{
    auto output =
        run(fmt::format("--es {} +count -e a_0.* +rename -r 'a_0.b_(#).(*)=$2$1' +summarize", build_es("patterns.in")));
    EXPECT_EQ(output, "foo0 1\nfoo1 1\nfoo2 1\nfoo3 1\n");
}

TEST_F(EVPTest, CanRenamePatternWithBackReference)
{
    auto output = run(
        fmt::format("--es {} +count -a +rename -r 'a_(#).b_(\\1).foo=match_$1' +summarize", build_es("patterns.in")));
    const auto* expected = R"(a_0.b_1.foo 1
a_0.b_2.foo 1
a_0.b_3.foo 1
a_1.b_0.foo 1
match_0     1
match_1     2
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanSum)
{
    auto output =
        run(fmt::format("--es {} +count -a +sum 'a_0.*=sum_0' 'a_1.*=sum_1' +summarize", build_es("patterns.in")));
    const auto* expected = R"(a_0.b_0.foo 1
a_0.b_1.foo 1
a_0.b_2.foo 1
a_0.b_3.foo 1
a_1.b_0.foo 1
a_1.b_1.foo 2
sum_0       4
sum_1       3
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanSumWithBackReference)
{
    auto output = run(fmt::format("--es {} +count -a +sum 'a_(#).*=sum_$1' +summarize", build_es("patterns.in")));
    const auto* expected = R"(a_0.b_0.foo 1
a_0.b_1.foo 1
a_0.b_2.foo 1
a_0.b_3.foo 1
a_1.b_0.foo 1
a_1.b_1.foo 2
sum_0       4
sum_1       3
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, MultipleSums)
{
    auto output =
        run(fmt::format("--es {} +count -a +sum '*.foo=sum_foo' '*.bar=sum_bar' +summarize", build_es("multi_sum.in")));
    const auto* expected = R"(sum_bar 3
sum_foo 4
x.bar   1
x.foo   3
y.bar   2
y.foo   1
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanGenerateTimeSeries)
{
    auto ts_file = test_file("ts.csv");
    auto output =
        run(fmt::format("--es {} -i 30 +count -a +summarize --timeseries {}", build_es("series.in"), ts_file));

    const auto* expected = R"(start_time,stop_time,a
10,29,3
30,59,5
60,89,3
90,90,1
)";

    auto actual = read_and_remove_file(ts_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, CanGenerateCompressedTimeSeries)
{
    auto ts_file = test_file("ts.csv.xz");
    auto output =
        run(fmt::format("--es {} -i 30 +count -a +summarize --timeseries {}", build_es("series.in"), ts_file));

    EXPECT_EQ(0, std::remove(ts_file.c_str()));
}

TEST_F(EVPTest, TimeSeriesIgnoresBeforeFirst)
{
    auto ts_file = test_file("ts.csv");
    auto output =
        run(fmt::format("--es {} -i 30 +count -a +summarize --timeseries {}", build_es("delayed_series.in"), ts_file));

    const auto* expected = R"(start_time,stop_time,a
40,59,3
60,89,5
90,119,3
120,120,1
)";

    auto actual = read_and_remove_file(ts_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, SummaryStillWorksWithTimeSeries)
{
    auto summary_file = test_file("summary.csv");
    auto output =
        run(fmt::format("--es {} -i 30 +count -a +summarize --summary {}", build_es("series.in"), summary_file));

    const auto* expected = "start_time,stop_time,a\n10,90,12\n";
    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, CanMeasureLatency)
{
    auto output = run(fmt::format("--es {} +latency a b +summarize", build_es("latency.in")));
    const auto* expected = R"(a_b.count           2
a_b.max_avg_latency 175
a_b.max_latency     250
a_b.min_latency     100
a_b.stdev           428.661
a_b.sum_latency     350
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanMeasureLatencyAndRenameOutput)
{
    auto output = run(fmt::format("--es {} +latency a b --name renamed +summarize", build_es("latency.in")));
    const auto* expected = R"(renamed.count           2
renamed.max_avg_latency 175
renamed.max_latency     250
renamed.min_latency     100
renamed.stdev           428.661
renamed.sum_latency     350
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanMeasureLatencyWithPrefix)
{
    auto output = run(fmt::format("--es {} +latency a b -p foo +summarize", build_es("latency.in")));
    const auto* expected = R"(a_b.count           1
a_b.max_avg_latency 280
a_b.max_latency     280
a_b.min_latency     280
a_b.stdev           0
a_b.sum_latency     280
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanMeasureLatencyWithPattern)
{
    auto output = run(fmt::format("--es {} +latency \\*a \\*b --name renamed +summarize", build_es("latency.in")));
    const auto* expected = R"(renamed.count           3
renamed.max_avg_latency 210
renamed.max_latency     280
renamed.min_latency     100
renamed.stdev           514.393
renamed.sum_latency     630
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, TwoLatenciesInOneTransaction)
{
    auto output = run(fmt::format("--es {} +latency multi_x multi_y +summarize", build_es("latency.in")));
    const auto* expected = R"(multi_x_multi_y.count           2
multi_x_multi_y.max_avg_latency 170
multi_x_multi_y.max_latency     170
multi_x_multi_y.min_latency     170
multi_x_multi_y.stdev           208.207
multi_x_multi_y.sum_latency     170
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanMeasureZeroLatency)
{
    auto output =
        run(fmt::format("--es {} +latency zero_lat_start zero_lat_end -n z +summarize", build_es("latency.in")));
    const auto* expected = R"(z.count       1
z.stdev       0
z.sum_latency 0
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyChain)
{
    auto output = run(fmt::format("--es {} +latency -n chain m n o p +summarize", build_es("latency.in")));
    const auto* expected = R"(chain.count             2
chain.m.count           2
chain.m.max_avg_latency 85
chain.m.max_latency     150
chain.m.min_latency     20
chain.m.stdev           208.207
chain.m.sum_latency     170
chain.max_avg_latency   85
chain.max_latency       150
chain.min_latency       20
chain.n.count           2
chain.n.max_avg_latency 335
chain.n.max_latency     350
chain.n.min_latency     320
chain.n.stdev           820.579
chain.n.sum_latency     670
chain.o.count           1
chain.o.max_avg_latency 510
chain.o.max_latency     510
chain.o.min_latency     510
chain.o.stdev           0
chain.o.sum_latency     510
chain.stdev             208.207
chain.sum_latency       170
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyWithAliasesChain)
{
    auto output = run(fmt::format("--es {} +latency -n chain m=M n=N o p=P +summarize", build_es("latency.in")));
    const auto* expected = R"(chain.M.count           2
chain.M.max_avg_latency 85
chain.M.max_latency     150
chain.M.min_latency     20
chain.M.stdev           208.207
chain.M.sum_latency     170
chain.N.count           2
chain.N.max_avg_latency 335
chain.N.max_latency     350
chain.N.min_latency     320
chain.N.stdev           820.579
chain.N.sum_latency     670
chain.count             2
chain.max_avg_latency   85
chain.max_latency       150
chain.min_latency       20
chain.o.count           1
chain.o.max_avg_latency 510
chain.o.max_latency     510
chain.o.min_latency     510
chain.o.stdev           0
chain.o.sum_latency     510
chain.stdev             208.207
chain.sum_latency       170
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyWithKeys)
{
    auto output = run(fmt::format("--es {} +latency -k x.b -k y.b x.a y.c +summarize", build_es("latency_keyed.in")));
    const auto* expected = R"(x.a_y_c.count           3
x.a_y_c.max_avg_latency 130
x.a_y_c.max_latency     130
x.a_y_c.min_latency     130
x.a_y_c.stdev           106.141
x.a_y_c.sum_latency     130
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyWithHistogram)
{
    auto output = run(fmt::format("--es {} +latency a b --histogram latency +summarize", build_es("latency.in")));
    const auto* expected = R"(a_b.count           2
a_b.latency.100     1
a_b.latency.250     1
a_b.max_avg_latency 175
a_b.max_latency     250
a_b.min_latency     100
a_b.stdev           428.661
a_b.sum_latency     350
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyWithFactoredHistogram)
{
    auto output =
        run(fmt::format("--es {} +latency a b --factored --histogram latency +summarize", build_es("latency.in")));
    const auto* expected = R"(a_b.count           2
a_b.max_avg_latency 175
a_b.max_latency     250
a_b.min_latency     100
a_b.stdev           428.661
a_b.sum_latency     350
a_b/latency:100     1
a_b/latency:250     1
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyWithFactoredBoundedHistogram)
{
    auto output = run(
        fmt::format("--es {} +latency a b --factored --histogram latency[::100] +summarize", build_es("latency.in")));
    const auto* expected = R"(a_b.count           2
a_b.max_avg_latency 175
a_b.max_latency     250
a_b.min_latency     100
a_b.stdev           428.661
a_b.sum_latency     350
a_b/latency:100     1
a_b/latency:200     1
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyIncludeRelatedParentChild)
{
    auto output = run(fmt::format("--es {} +latency --include-related -n pc parent_a child_b +summarize",
                                  build_es("latency_related.in")));
    const auto* expected = R"(pc.count           1
pc.max_avg_latency 20
pc.max_latency     20
pc.min_latency     20
pc.stdev           0
pc.sum_latency     20
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyIncludeRelatedChildParent)
{
    auto output = run(fmt::format("--es {} +latency --include-related -n cp child_a parent_b +summarize",
                                  build_es("latency_related.in")));
    const auto* expected = R"(cp.count           1
cp.max_avg_latency 10
cp.max_latency     10
cp.min_latency     10
cp.stdev           0
cp.sum_latency     10
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyIncludeRelatedGrandparentGrandchild)
{
    auto output = run(fmt::format("--es {} +latency --include-related -n gg grandparent_a grandchild_b +summarize",
                                  build_es("latency_related.in")));
    const auto* expected = R"(gg.count           1
gg.max_avg_latency 10
gg.max_latency     10
gg.min_latency     10
gg.stdev           0
gg.sum_latency     10
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyIncludeRelatedExcludesSiblings)
{
    auto output = run(fmt::format("--es {} +latency --include-related -n sib sibling_a sibling_b +summarize",
                                  build_es("latency_related.in")));
    const auto* expected = R"(sib.count       0
sib.stdev       0
sib.sum_latency 0
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyIncludeRelatedPreservesSameTxid)
{
    auto output = run(fmt::format("--es {} +latency --include-related -n same same_a same_b +summarize",
                                  build_es("latency_related.in")));
    const auto* expected = R"(same.count           1
same.max_avg_latency 10
same.max_latency     10
same.min_latency     10
same.stdev           0
same.sum_latency     10
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyIncludeRelatedChain)
{
    auto output =
        run(fmt::format("--es {} +latency --include-related -n rc related_chain_a related_chain_b "
                        "related_chain_c +summarize",
                        build_es("latency_related.in")));
    const auto* expected = R"(rc.count                           1
rc.max_avg_latency                 20
rc.max_latency                     20
rc.min_latency                     20
rc.related_chain_a.count           1
rc.related_chain_a.max_avg_latency 20
rc.related_chain_a.max_latency     20
rc.related_chain_a.min_latency     20
rc.related_chain_a.stdev           0
rc.related_chain_a.sum_latency     20
rc.related_chain_b.count           1
rc.related_chain_b.max_avg_latency 30
rc.related_chain_b.max_latency     30
rc.related_chain_b.min_latency     30
rc.related_chain_b.stdev           0
rc.related_chain_b.sum_latency     30
rc.stdev                           0
rc.sum_latency                     20
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyIncludeRelatedChainExcludesSiblingLeg)
{
    auto output =
        run(fmt::format("--es {} +latency --include-related -n rsc related_sibling_chain_a "
                        "related_sibling_chain_b related_sibling_chain_c +summarize",
                        build_es("latency_related.in")));
    const auto* expected = R"(rsc.count                                   1
rsc.max_avg_latency                         20
rsc.max_latency                             20
rsc.min_latency                             20
rsc.related_sibling_chain_a.count           1
rsc.related_sibling_chain_a.max_avg_latency 20
rsc.related_sibling_chain_a.max_latency     20
rsc.related_sibling_chain_a.min_latency     20
rsc.related_sibling_chain_a.stdev           0
rsc.related_sibling_chain_a.sum_latency     20
rsc.related_sibling_chain_b.count           0
rsc.related_sibling_chain_b.stdev           0
rsc.related_sibling_chain_b.sum_latency     0
rsc.stdev                                   0
rsc.sum_latency                             20
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyIncludeRelatedRetiresEndedTransactions)
{
    auto output = run(fmt::format("--es {} +latency --include-related -n stale stale_a stale_b +summarize",
                                  build_es("latency_related.in")));
    const auto* expected = R"(stale.count       0
stale.stdev       0
stale.sum_latency 0
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyIncludeRelatedRepeatedStartReplacesPreviousStart)
{
    auto output =
        run(fmt::format("--es {} +latency --include-related -n rel_repeat related_repeat_a related_repeat_b "
                        "+summarize",
                        build_es("latency_related.in")));
    const auto* expected = R"(rel_repeat.count           1
rel_repeat.max_avg_latency 30
rel_repeat.max_latency     30
rel_repeat.min_latency     30
rel_repeat.stdev           0
rel_repeat.sum_latency     30
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, LatencyIncludeRelatedRejectsKeys)
{
    auto output = run_expecting_error(fmt::format(
        "--es {} +latency --include-related -k txid parent_a child_b +summarize", build_es("latency_related.in")));
    EXPECT_PLUGIN_ERROR(output, "latency", "--include-related cannot be combined with -k|--key\n");
}

TEST_F(EVPTest, CanMeasureOccupancy)
{
    auto output = run(fmt::format("--es {} +occupancy alloc dealloc +summarize", build_es("occupancy.in")));
    const auto* expected = R"(occupancy.0 30
occupancy.1 45
occupancy.2 15
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanMeasureNamedOccupancy)
{
    auto output = run(fmt::format("--es {} +occupancy -n slots alloc dealloc +summarize", build_es("occupancy.in")));
    const auto* expected = R"(slots.0 30
slots.1 45
slots.2 15
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanMeasureFactoredOccupancy)
{
    auto output =
        run(fmt::format("--es {} +occupancy --factored -n slots alloc dealloc +summarize", build_es("occupancy.in")));
    const auto* expected = R"(slots/occupancy:0 30
slots/occupancy:1 45
slots/occupancy:2 15
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanMeasureOccupancyTimeSeries)
{
    auto ts_file = test_file("ts.csv");
    auto output = run(fmt::format(
        "--es {} -i 30 +occupancy alloc dealloc +summarize --timeseries {}", build_es("occupancy.in"), ts_file));

    const auto* expected = R"(start_time,stop_time,occupancy.0,occupancy.1,occupancy.2
10,29,0,10,9
30,59,9,15,6
60,89,21,9,0
90,100,0,11,0
)";

    auto actual = read_and_remove_file(ts_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, CanMeasureOccupancyWithTimeInterval)
{
    auto output = run(fmt::format("--es {} +occupancy 30 alloc dealloc +summarize", build_es("occupancy.in")));
    const auto* expected = R"(occupancy.0 2
occupancy.1 1
occupancy.2 1
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, CanMeasureOccupancyWithTimeIntervalSuffix)
{
    auto output = run(fmt::format("--es {} +occupancy 30ps alloc dealloc +summarize", build_es("occupancy.in")));
    const auto* expected = R"(occupancy.0 2
occupancy.1 1
occupancy.2 1
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, OccupancyWithTimeIntervalDoesNotAdjustAfterFinalBoundary)
{
    auto output = run(fmt::format("--es {} +occupancy 50 alloc dealloc +summarize", build_es("occupancy.in")));
    EXPECT_EQ(output, "occupancy.1 2\n");
}

TEST_F(EVPTest, CanMeasureOccupancyWithEventInterval)
{
    auto output =
        run(fmt::format("--es {} +occupancy marker alloc dealloc +summarize", build_es("occupancy_event_interval.in")));
    const auto* expected = R"(occupancy.0 2
occupancy.1 2
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, OccupancyWithZeroTimeIntervalIsAnError)
{
    auto output =
        run_expecting_error(fmt::format("--es {} +occupancy 0 alloc dealloc +summarize", build_es("occupancy.in")));
    EXPECT_PLUGIN_ERROR(output, "occupancy", "occupancy interval must be greater than zero\n");
}

TEST_F(EVPTest, Python)
{
    auto output = run(fmt::format("--es {} +python {}", build_es("basic.in"), config("pytest.py")));
    const auto* expected = R"(saw one at 100
event one at 100 with data1=0 and data2=0
event two at 100 with data1=0 and data2=0
saw one at 200
event one at 200 with data1=42 and data2=903
event two at 300 with data1=21 and data2=760
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, MissingPythonFile)
{
    auto output = run_expecting_error(fmt::format("--es {} +python missing.py", build_es("basic.in")));
    EXPECT_PLUGIN_ERROR(output, "python", "could not open file missing.py\n");
}

TEST_F(EVPTest, PythonFilesWontShare)
{
    auto output = run_expecting_error(
        fmt::format("--es {} +python {} +python {}", build_es("basic.in"), config("one.py"), config("two.py")));
    EXPECT_THAT(output, ::testing::HasSubstr("name 'x' is not defined"));
}

TEST_F(EVPTest, PythonArgumentPassing)
{
    auto output = run(fmt::format("--es {} +python {} Raleigh --time 6pm +python {} --hello 'Portland' --time 3pm",
                                  build_es("basic.in"),
                                  config("arguments.py"),
                                  config("arguments.py")));
    EXPECT_THAT(output, ::testing::HasSubstr("Goodbye, Raleigh! It is 6pm"));
    EXPECT_THAT(output, ::testing::HasSubstr("Hello, Portland! It is 3pm"));
}

TEST_F(EVPTest, CanCollectFromPython)
{
    auto ts_file = test_file("ts.csv");
    auto output = run(fmt::format("--es {} -i 30 +count -a +summarize --timeseries {} +python {} +python {}",
                                  build_es("series.in"),
                                  ts_file,
                                  config("collect.py"),
                                  config("one.py")));

    const auto* expected = R"(start_time,stop_time,a,time
10,29,3,29
30,59,5,30
60,89,3,30
90,90,1,1
)";

    auto actual = read_and_remove_file(ts_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, GetParamFromPython)
{
    auto output = run(fmt::format("--es {} -i 30 +count -a +python {}", build_es("factors.in"), config("param.py")));

    const auto* expected = R"(int 4
int 192
list [7, 4]
NoneType None
)";

    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, PythonTransactionQueries)
{
    auto output = run(fmt::format("--es {} +python {}", build_es("transaction_queries.in"), config("transactions.py")));

    const auto* expected = R"(probe txid=1 parent=None root=False related_2_3=False related_1=True unrelated_99=False
probe txid=2 parent=1 root=True related_2_3=False related_1=True unrelated_99=False
probe txid=3 parent=1 root=True related_2_3=False related_1=True unrelated_99=False
probe txid=4 parent=2 root=True related_2_3=False related_1=True unrelated_99=False
no_tx txid=None
)";

    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, PythonTransactionQueriesEndParentsAfterCallback)
{
    auto output = run(fmt::format("--es {} +python {}", build_es("transaction_end.in"), config("transactions_end.py")));

    const auto* expected = R"(end_transaction txid=2 parent=1
probe_after_end parent_2=None related_1_2=False
end_transaction txid=1 parent=None
)";

    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, PythonTransactionQueriesBoundCyclicParents)
{
    auto output =
        run(fmt::format("--es {} +python {}", build_es("transaction_cycle.in"), config("transactions_cycle.py")));

    const auto* expected = R"(probe_self parent_1=1 self_ancestor=True unrelated_99=False
probe_parent_child parent_1=2 parent_2=1 root_2=True cycle_1_2=True related_1_2=True
probe_cycle parent_3=4 parent_4=3 ancestor_4_3=True ancestor_3_4=True unrelated_99_4=False
)";

    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, PythonTransactionQueriesRequireOptIn)
{
    auto output = run_expecting_error(
        fmt::format("--es {} +python {}", build_es("transaction_queries.in"), config("transactions_no_require.py")));

    EXPECT_THAT(output, ::testing::HasSubstr("evp transaction queries require evp.require_transactions()"));
}

TEST_F(EVPTest, TestVariables)
{
    auto output =
        run(fmt::format("--es {} -s hello_to=world -P python {}", build_es("basic.in"), config("say_hello.py")));
    EXPECT_EQ(output, "hello world\n");
}

TEST_F(EVPTest, ExpandsParameterInParamPluginArgument)
{
    auto output =
        run(fmt::format("--es {} +param -p '{{{{ param.report_param }}}}' +summarize", build_es("expansion.in")));
    EXPECT_EQ(output, "three four\n");
}

TEST_F(EVPTest, ExpandsParameterSubstringsInPythonArguments)
{
    auto output =
        run(fmt::format("--es {} +python {} 'prefix-{{{{param.core.machine_width}}}}' "
                        "--time '{{{{param.core.rob.size}}}}'",
                        build_es("expansion.in"),
                        config("arguments.py")));
    EXPECT_EQ(output, "Goodbye, prefix-4! It is 192\n");
}

TEST_F(EVPTest, ExpandsMultipleParametersInOneArgument)
{
    auto output =
        run(fmt::format("--es {} +python {} "
                        "'{{{{ param.core.machine_width }}}}-{{{{ param.three }}}}-"
                        "{{{{ param.core.machine_width }}}}' --time noon",
                        build_es("expansion.in"),
                        config("arguments.py")));
    EXPECT_EQ(output, "Goodbye, 4-four-4! It is noon\n");
}

TEST_F(EVPTest, ExpandsParameterInVariables)
{
    auto output = run(fmt::format("--es {} -s hello_to='{{{{ param.hello_value }}}}' -P python {}",
                                  build_es("expansion.in"),
                                  config("say_hello.py")));
    EXPECT_EQ(output, "hello world\n");
}

TEST_F(EVPTest, ExpandsArgumentInPluginArgument)
{
    auto output = run(fmt::format("--es {} -s location=Portland +python {} '{{{{ location }}}}' --time noon",
                                  build_es("expansion.in"),
                                  config("arguments.py")));
    EXPECT_EQ(output, "Goodbye, Portland! It is noon\n");
}

TEST_F(EVPTest, ExpandsArgumentFromParameterInPluginArgument)
{
    auto output =
        run(fmt::format("--es {} -s width='{{{{ param.core.machine_width }}}}' "
                        "+python {} 'width-{{{{ width }}}}' --time noon",
                        build_es("expansion.in"),
                        config("arguments.py")));
    EXPECT_EQ(output, "Goodbye, width-4! It is noon\n");
}

TEST_F(EVPTest, ExpandsParameterInStart)
{
    auto summary_file = test_file("summary.csv");
    auto output = run(fmt::format("--es {} --start '{{{{ param.start_time }}}}' +count -a +summarize --summary {}",
                                  build_es("expansion.in"),
                                  summary_file));

    const auto* expected = R"(start_time,stop_time,one,two
200,300,1,1
)";

    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, ExpandsArgumentInStart)
{
    auto summary_file = test_file("summary.csv");
    auto output = run(fmt::format("--es {} -s begin=200 --start '{{{{ begin }}}}' +count -a +summarize --summary {}",
                                  build_es("expansion.in"),
                                  summary_file));

    const auto* expected = R"(start_time,stop_time,one,two
200,300,1,1
)";

    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, MissingExpansionParameterIsAnError)
{
    auto output = run_expecting_error(fmt::format(
        "--es {} +python {} '{{{{ param.does.not.exist }}}}'", build_es("basic.in"), config("arguments.py")));
    EXPECT_THAT(output, ::testing::HasSubstr("Error: "));
    EXPECT_THAT(output, ::testing::HasSubstr("parameter \"does.not.exist\" not found"));
}

TEST_F(EVPTest, MissingExpansionArgumentIsAnError)
{
    auto output = run_expecting_error(
        fmt::format("--es {} +python {} '{{{{ missing }}}}'", build_es("basic.in"), config("arguments.py")));
    EXPECT_THAT(output, ::testing::HasSubstr("Error: "));
    EXPECT_THAT(output, ::testing::HasSubstr("argument \"missing\" not found"));
}

TEST_F(EVPTest, EmptyExpansionIsAnError)
{
    auto output = run_expecting_error(
        fmt::format("--es {} +python {} '{{{{ }}}}'", build_es("basic.in"), config("arguments.py")));
    EXPECT_THAT(output, ::testing::HasSubstr("Error: "));
    EXPECT_THAT(output, ::testing::HasSubstr("empty argument expansion"));
}

TEST_F(EVPTest, UnmatchedExpansionIsAnError)
{
    auto output = run_expecting_error(
        fmt::format("--es {} +python {} '{{{{ missing'", build_es("basic.in"), config("arguments.py")));
    EXPECT_THAT(output, ::testing::HasSubstr("Error: "));
    EXPECT_THAT(output, ::testing::HasSubstr("unmatched \"{{\""));
}

TEST_F(EVPTest, ParamPrefixIsReservedForArguments)
{
    auto output = run_expecting_error(fmt::format("--es {} -s param.foo=bar +count -a", build_es("basic.in")));
    EXPECT_THAT(output, ::testing::HasSubstr("Error: "));
    EXPECT_THAT(output, ::testing::HasSubstr("reserved prefix \"param.\""));
}

TEST_F(EVPTest, CyclicArgumentExpansionIsAnError)
{
    auto output = run_expecting_error(
        fmt::format("--es {} -s one='{{{{ two }}}}' -s two='{{{{ one }}}}' +count -a", build_es("basic.in")));
    EXPECT_THAT(output, ::testing::HasSubstr("Error: "));
    EXPECT_THAT(output, ::testing::HasSubstr("cycle detected while expanding argument"));
}

TEST_F(EVPTest, TestHelp)
{
    auto output_h = run_expecting_error("-h");
    auto output_help = run_expecting_error("--help");

    EXPECT_EQ(output_h, output_help);
    EXPECT_THAT(output_help, ::testing::HasSubstr("--es FILENAME.es"));
}

TEST_F(EVPTest, Dump)
{
    auto output = run_expecting_error(
        "--dump --start 0 +count -e xxx +summarize --summary one.csv +summarize --summary two.csv +count -e yyy");
    const auto* expected_output = R"(global options:
  --start 0

count[0] plugin options:
  -e xxx
  -e yyy

summarize[0] plugin options:
  --summary one.csv

summarize[1] plugin options:
  --summary two.csv


)";

    EXPECT_EQ(output, expected_output);
}

TEST_F(EVPTest, MissingPlugin)
{
    auto output = run_expecting_error(fmt::format("--es {} -P noplugin", build_es("basic.in")));
    EXPECT_THAT(output, ::testing::HasSubstr("could not find plugin \"noplugin\""));
}

TEST_F(EVPTest, MissingPluginArg)
{
    auto output = run_expecting_error(fmt::format("--es {} -P", build_es("basic.in")));
    EXPECT_THAT(output, ::testing::HasSubstr("missing plugin name after \"-P\""));
}

TEST_F(EVPTest, CanReadCompressedFile)
{
    auto output = run(fmt::format("--es {} +count -a +summarize", build_es_xz("basic.in")));
    EXPECT_EQ(output, "one 2\ntwo 2\n");
}

TEST_F(EVPTest, CanCapture)
{
    auto captured = test_file("captured.es");
    auto output = run(fmt::format("--es {} +count -a +capture {}", build_es("factors.in"), captured));
    EXPECT_EQ(
        output,
        "Captured 1 event definitions, 2 value definitions, 0 enum definitions, 3 parameter values, and 10 events.\n");
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CanCaptureOverwrite)
{
    auto captured = test_file("captured.es");
    auto output = run(fmt::format("--es {} +count -a +capture {}", build_es("factors.in"), captured));
    EXPECT_EQ(
        output,
        "Captured 1 event definitions, 2 value definitions, 0 enum definitions, 3 parameter values, and 10 events.\n");

    output = run(fmt::format("--es {} +count -a +capture {} --force", build_es("factors.in"), captured));
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CanCaptureCompressed)
{
    auto captured = test_file("captured.es.xz");
    auto output = run(fmt::format("--es {} +count -a +capture {}", build_es("factors.in"), captured));
    EXPECT_EQ(
        output,
        "Captured 1 event definitions, 2 value definitions, 0 enum definitions, 3 parameter values, and 10 events.\n");
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CanCaptureNumericalPrefix)
{
    auto captured = test_file("captured.es", "403");
    auto output = run(fmt::format("--es {} +count -a +capture {}", build_es("factors.in"), captured));
    EXPECT_EQ(
        output,
        "Captured 1 event definitions, 2 value definitions, 0 enum definitions, 3 parameter values, and 10 events.\n");
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CanCaptureWithFilter)
{
    auto captured = test_file("captured.es");
    auto output = run(fmt::format("--es {} +count -a +capture {} --filter b=1", build_es("factors.in"), captured));
    EXPECT_EQ(
        output,
        "Captured 1 event definitions, 2 value definitions, 0 enum definitions, 3 parameter values, and 2 events.\n");
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CanCaptureWithAnyFilter)
{
    auto captured = test_file("captured.es");
    auto output =
        run(fmt::format("--es {} +count -a +capture {} --filter a=2 --filter b=2", build_es("factors.in"), captured));
    EXPECT_EQ(
        output,
        "Captured 1 event definitions, 2 value definitions, 0 enum definitions, 3 parameter values, and 4 events.\n");
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CanCaptureWithRepeatedFilterAlternatives)
{
    auto captured = test_file("captured.es");
    auto output =
        run(fmt::format("--es {} +count -a +capture {} --filter a=2 --filter a=3", build_es("factors.in"), captured));
    EXPECT_EQ(
        output,
        "Captured 1 event definitions, 2 value definitions, 0 enum definitions, 3 parameter values, and 5 events.\n");
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CanCaptureWithAllFilters)
{
    auto captured = test_file("captured.es");
    auto output = run(fmt::format(
        "--es {} +count -a +capture {} --all-filters --filter a=2 --filter b=1", build_es("factors.in"), captured));
    EXPECT_EQ(
        output,
        "Captured 1 event definitions, 2 value definitions, 0 enum definitions, 3 parameter values, and 2 events.\n");
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CanCaptureWithAllFiltersAndAlternatives)
{
    auto captured = test_file("captured.es");
    auto output = run(fmt::format("--es {} +count -a +capture {} --all-filters --filter a=2 --filter a=3 --filter b=2",
                                  build_es("factors.in"),
                                  captured));
    EXPECT_EQ(
        output,
        "Captured 1 event definitions, 2 value definitions, 0 enum definitions, 3 parameter values, and 1 events.\n");
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CanCaptureEnums)
{
    auto captured = test_file("captured.es");
    auto output = run(fmt::format("--es {} +count -a +capture {}", build_es("enums.in"), captured));
    EXPECT_EQ(
        output,
        "Captured 1 event definitions, 1 value definitions, 1 enum definitions, 0 parameter values, and 3 events.\n");
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CanCaptureToStdout)
{
    auto output = run(fmt::format("--es {} +count -a +capture -", build_es("factors.in")));
    EXPECT_GT(output.size(), 16);
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Captured 1 event definitions")));
}

TEST_F(EVPTest, TimeSkip)
{
    auto captured = test_file("captured.es");
    auto output =
        run(fmt::format("--es {} +count -a +time_skip {} --event clock_skip", build_es("skipping.in"), captured));
    const auto* expected =
        R"(Captured 1 event definitions, 0 value definitions, 0 enum definitions, 0 parameter values, and 3 events.
Skipped a total of 30ps via clock_skip
)";
    EXPECT_EQ(output, expected);
    std::remove(captured.c_str());
}

TEST_F(EVPTest, CountImmediatelyAtTimeZero)
{
    auto summary_file = test_file("summary.csv");
    auto output =
        run(fmt::format("--es {} --start 0 +count -a +summarize --summary {}", build_es("basic.in"), summary_file));

    const auto* expected = R"(start_time,stop_time,one,two
100,300,2,2
)";

    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, CountAtStart)
{
    auto summary_file = test_file("summary.csv");
    auto output =
        run(fmt::format("--es {} --start 200 +count -a +summarize --summary {}", build_es("basic.in"), summary_file));

    const auto* expected = R"(start_time,stop_time,one,two
200,300,1,1
)";

    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, CountAfterStart)
{
    auto summary_file = test_file("summary.csv");
    auto output =
        run(fmt::format("--es {} --start 63 +count -a +summarize --summary {}", build_es("series.in"), summary_file));

    const auto* expected = R"(start_time,stop_time,a
70,90,3
)";

    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, CountBeforeStop)
{
    auto summary_file = test_file("summary.csv");
    auto output =
        run(fmt::format("--es {} --stop 43 +count -a +summarize --summary {}", build_es("series.in"), summary_file));

    const auto* expected = R"(start_time,stop_time,a
10,44,6
)";

    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, CountBetweenStartStop)
{
    auto summary_file = test_file("summary.csv");
    auto output = run(fmt::format(
        "--es {} --start 27 --stop 75 +count -a +summarize --summary {}", build_es("series.in"), summary_file));

    const auto* expected = R"(start_time,stop_time,a
30,79,7
)";

    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, CountImmediatelyAtNoEvents)
{
    auto summary_file = test_file("summary.csv");
    auto output = run(fmt::format(
        "--es {} --start a=0 --stop a=10 +count -a +summarize --summary {}", build_es("series.in"), summary_file));

    const auto* expected = R"(start_time,stop_time,a
10,69,10
)";
    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, CountBetweenStartStopEvents)
{
    auto summary_file = test_file("summary.csv");
    auto output = run(fmt::format(
        "--es {} --start a=3 --stop a=10 +count -a +summarize --summary {}", build_es("series.in"), summary_file));

    const auto* expected = R"(start_time,stop_time,a
30,69,7
)";
    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
}

TEST_F(EVPTest, ExitAfterStop)
{
    auto summary_file = test_file("summary.csv");
    auto output = run(
        fmt::format("--es {} --stop 43 --exit +count -a +summarize --summary {}", build_es("series.in"), summary_file));

    const auto* expected = R"(start_time,stop_time,a
10,44,6
)";

    auto actual = read_and_remove_file(summary_file);
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(output, "Stopping: exiting on stop\n");
}

TEST_F(EVPTest, ReportParams)
{
    auto output = run(fmt::format("--es {} +param -a +summarize", build_es("basic.in")));
    const auto* expected = R"(core.machine_width           4
core.rob.size                192
core.scheduler_configuration [7,4]
three                        "four"
)";
    EXPECT_EQ(output, expected);
}

TEST_F(EVPTest, ReportParamsFiltered)
{
    auto output = run(fmt::format("--es {} +param -p three +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "three \"four\"\n");
}

TEST_F(EVPTest, ReportParamsFilteredGlob)
{
    auto output = run(fmt::format("--es {} +param -p core.sch* +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "core.scheduler_configuration [7,4]\n");
}

TEST_F(EVPTest, ReportParamsWithEventCounts)
{
    auto output = run(fmt::format("--es {} +count -a +param -p three +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "one   2\nthree \"four\"\ntwo   2\n");
}

TEST_F(EVPTest, CanReadFromConfigFile)
{
    auto output = run(fmt::format("--es {} @{} +summarize", build_es("basic.in"), config("basic.cfg")));
    EXPECT_EQ(output, "two 2\n");
}

TEST_F(EVPTest, CanReadFromConfigFileWithComments)
{
    auto output = run(fmt::format("--es {} @{} +summarize", build_es("basic.in"), config("comment.cfg")));
    EXPECT_EQ(output, "two 2\n");
}

TEST_F(EVPTest, MissingConfigFile)
{
    run_expecting_error(fmt::format("--es {} @missing.cfg +summarize", build_es("basic.in")));
}

TEST_F(EVPTest, CanReadFromInstalledConfigFile)
{
    auto output = run(fmt::format("--es {} @all.cfg +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "one 2\ntwo 2\n");
}

TEST_F(EVPTest, ExplicitRelativeDoesNotSearchConfigPath)
{
    auto output = run_expecting_error(fmt::format("--es {} @./all.cfg +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "Error: could not find config file \"./all.cfg\"\n");
}

TEST_F(EVPTest, ExplicitParentRelativeDoesNotSearchConfigPath)
{
    auto output =
        run_expecting_error(fmt::format("--es {} @../events.processor/all.cfg +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "Error: could not find config file \"../events.processor/all.cfg\"\n");
}

TEST_F(EVPTest, CanReadFromChainedConfigFile)
{
    auto output = run(fmt::format("--es {} @{} +summarize", build_es("basic.in"), config("chained.cfg")));
    EXPECT_EQ(output, "one 2\ntwo 2\n");
}

TEST_F(EVPTest, CanMeasureRateWithinTime)
{
    auto output = run(fmt::format("--es {} +rate 30 -e a +summarize", build_es("series.in")));
    EXPECT_EQ(output, "a.rate.3 2\na.rate.5 1\n");
}

TEST_F(EVPTest, CanMeasureRateWithinTimeSuffix)
{
    auto output = run(fmt::format("--es {} +rate 30ps -e a +summarize", build_es("series.in")));
    EXPECT_EQ(output, "a.rate.3 2\na.rate.5 1\n");
}

TEST_F(EVPTest, CanMeasureRateWithinEmptyTimeIntervals)
{
    auto output = run(fmt::format("--es {} +rate 30 -e a +summarize", build_es("gapped_series.in")));
    EXPECT_EQ(output, "a.rate.0 2\na.rate.1 2\n");
}

TEST_F(EVPTest, CanMeasureRateWithinEmptyFinalTimeInterval)
{
    auto output = run(fmt::format("--es {} +rate 100 -e one +summarize", build_es("basic.in")));
    EXPECT_EQ(output, "one.rate.1 2\n");
}

TEST_F(EVPTest, RateWithZeroTimeIntervalIsAnError)
{
    auto output = run_expecting_error(fmt::format("--es {} +rate 0 -e a +summarize", build_es("series.in")));
    EXPECT_PLUGIN_ERROR(output, "rate", "rate interval must be greater than zero\n");
}

TEST_F(EVPTest, CanNameRateWithShortOption)
{
    auto output = run(fmt::format("--es {} +rate 30 -n named -e a +summarize", build_es("series.in")));
    EXPECT_EQ(output, "a.named.3 2\na.named.5 1\n");
}

TEST_F(EVPTest, CanNameRateWithLongOption)
{
    auto output = run(fmt::format("--es {} +rate 30 --name named -e a +summarize", build_es("series.in")));
    EXPECT_EQ(output, "a.named.3 2\na.named.5 1\n");
}

TEST_F(EVPTest, CanMeasureRateWithinTimeFactored)
{
    auto output = run(fmt::format("--es {} +rate 30 --factored -e a +summarize", build_es("series.in")));
    EXPECT_EQ(output, "a/rate:3 2\na/rate:5 1\n");
}

TEST_F(EVPTest, CanMeasureRateWithinTimeFactoredBounded)
{
    auto output = run(fmt::format("--es {} +rate 30 --factored -e a[::2] +summarize", build_es("series.in")));
    EXPECT_EQ(output, "a/rate:2 2\na/rate:4 1\n");
}

TEST_F(EVPTest, CanMeasureRateWithinEvent)
{
    auto output = run(fmt::format("--es {} +rate b -e a +summarize", build_es("multiple_series.in")));
    EXPECT_EQ(output, "a.rate.0 1\na.rate.1 3\na.rate.2 1\na.rate.3 1\na.rate.4 1\n");
}

TEST_F(EVPTest, CanMeasureRateWithinEventBounded)
{
    auto output = run(fmt::format("--es {} +rate b -e a[1:4:2] +summarize", build_es("multiple_series.in")));
    EXPECT_EQ(output, "a.rate.1 4\na.rate.2 2\na.rate.4 1\n");
}

TEST_F(EVPTest, CanConvertFactoredEnums)
{
    auto output = run(fmt::format("--es {} +count -e ev/myenum +summarize", build_es("enums.in")));
    EXPECT_EQ(output, R"(ev          3
ev/myenum:a 1
ev/myenum:b 1
ev/myenum:c 1
)");
}

TEST_F(EVPTest, CanNotConvertFactoredEnums)
{
    auto output = run(fmt::format("--es {} +count --no-enum -e ev/myenum +summarize", build_es("enums.in")));
    EXPECT_EQ(output, R"(ev          3
ev/myenum:1 1
ev/myenum:2 1
ev/myenum:3 1
)");
}

TEST_F(EVPTest, CanConvertEnumsAndPrint)
{
    auto output = run(fmt::format("--es {} +count -a +print", build_es("enums.in")));
    EXPECT_EQ(output, R"(100 ev myenum=a
200 ev myenum=b
300 ev myenum=c
)");
}

TEST_F(EVPTest, CanNotConvertEnumsAndPrint)
{
    auto output = run(fmt::format("--es {} +count -a +print --no-enum", build_es("enums.in")));
    EXPECT_EQ(output, R"(100 ev myenum=1
200 ev myenum=2
300 ev myenum=3
)");
}

TEST_F(EVPTest, CanLsEvents)
{
    auto output = run(fmt::format("--es {} +ls", build_es("basic.in")));
    EXPECT_EQ(output, R"(event one event
event two event
)");
}

TEST_F(EVPTest, CanLsValues)
{
    auto output = run(fmt::format("--es {} +ls --values", build_es("basic.in")));
    EXPECT_EQ(output, R"(value data1 data
value data2 data
)");
}

TEST_F(EVPTest, CanLsEnums)
{
    auto output = run(fmt::format("--es {} +ls --enum myenum", build_es("enums.in")));
    EXPECT_EQ(output, R"(value myenum
enum  value[1] a
enum  value[2] b
enum  value[3] c
)");
}

TEST_F(EVPTest, PythonDoesNotCreateEnumTypesForNonEnums)
{
    auto output = run(fmt::format("--es {} +python {} one", build_es("basic.in"), config("enums.py")));
    EXPECT_EQ(output, R"(100 one
200 one data1(i)=42, data2(i)=903
)");
}

TEST_F(EVPTest, PythonDoesNotCreateEnumTypesForNonEnumsAndStrWorksAsExpected)
{
    auto output = run(fmt::format("--es {} +python {} one --expand-enums", build_es("basic.in"), config("enums.py")));
    EXPECT_EQ(output, R"(100 one
200 one data1(i)=42, data2(i)=903
)");
}

TEST_F(EVPTest, PythonDoesNotConvertEnumsByDefault)
{
    auto output = run(fmt::format("--es {} +python {} ev", build_es("enums.in"), config("enums.py")));
    EXPECT_EQ(output, R"(100 ev myenum(E)=1
200 ev myenum(E)=2
300 ev myenum(E)=3
)");
}

TEST_F(EVPTest, PythonConvertEnumsIfConfigured)
{
    auto output = run(fmt::format("--es {} +python {} ev --expand-enums", build_es("enums.in"), config("enums.py")));
    EXPECT_EQ(output, R"(100 ev myenum(E)=a
200 ev myenum(E)=b
300 ev myenum(E)=c
)");
}

TEST_F(EVPTest, PythonEnumIntEquality)
{
    auto output = run(fmt::format("--es {} +python {} ev --eq 2", build_es("enums.in"), config("enums.py")));
    EXPECT_EQ(output, R"(100 ev myenum(E)=1
200 ev myenum(E)=2
equal: myenum=b == 2
300 ev myenum(E)=3
)");
}

TEST_F(EVPTest, PythonEnumStrEquality)
{
    auto output =
        run(fmt::format("--es {} +python {} ev --expand-enums --eq b", build_es("enums.in"), config("enums.py")));
    EXPECT_EQ(output, R"(100 ev myenum(E)=a
200 ev myenum(E)=b
equal: myenum=b == b
300 ev myenum(E)=c
)");
}

TEST_F(EVPTest, PythonEnumsDict)
{
    auto output =
        run(fmt::format("--es {} +python {} ev --expect-enum myenum", build_es("enums.in"), config("enums.py")));
    EXPECT_EQ(output, R"(100 ev myenum(E)=1
200 ev myenum(E)=2
300 ev myenum(E)=3
)");
}
