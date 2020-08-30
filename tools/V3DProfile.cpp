/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "Mailbox.h"
#include "common.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <iomanip>
#include <iostream>
#include <thread>

#if __has_include(<ncurses.h>)
#include <ncurses.h>
#elif __has_include(<ncurses/ncurses.h>)
#include <ncurses/ncurses.h>
#elif __has_include(<ncurses/curses.h>)
#include <ncurses/curses.h>
#else
#define NO_NCURSES 1
#endif

#ifndef NO_NCURSES
// NCurses usage taken from here:
// https://www.viget.com/articles/game-programming-in-c-with-the-ncurses-library/

using namespace vc4cl;

static constexpr int COUNTER_IDLE = 0;
static constexpr int COUNTER_EXECUTIONS = 1;
static constexpr int COUNTER_TMU_STALLS = 2;
static constexpr int COUNTER_INSTRUCTION_CACHE_HITS = 3;
static constexpr int COUNTER_INSTRUCTION_CACHE_MISSES = 4;
static constexpr int COUNTER_UNIFORM_CACHE_HITS = 5;
static constexpr int COUNTER_UNIFORM_CACHE_MISSES = 6;
static constexpr int COUNTER_VPW_STALLS = 7;
static constexpr int COUNTER_VPR_STALLS = 8;
static constexpr int COUNTER_L2_HITS = 9;
static constexpr int COUNTER_L2_MISSES = 10;
static constexpr int COUNTER_TMU_TOTAL_LOADS = 11;
static constexpr int COUNTER_TMU_CACHE_MISSES = 12;

static constexpr char execChar = '=';
static constexpr char tmuStallChar = '*';
static constexpr char vpmStallChar = '~';

static constexpr auto pollInterval = std::chrono::milliseconds{500};
static constexpr auto intervalsPerSecond = std::chrono::seconds{1} / pollInterval;

static void checkResult(bool result)
{
    if(!result)
        throw std::runtime_error("Error in V3D query!");
}

static int toWidth(int64_t val, int64_t total, int64_t totalWidth)
{
    // width = val / total * totalWidth
    // width = val * totalWidth / total (prevents requirement of float)
    if(val <= 0 || total <= 0 || totalWidth <= 0)
        return 0;
    return static_cast<int>((val * totalWidth) / total);
}

static double toPercent(int64_t val, int64_t total)
{
    auto part = static_cast<double>(val);
    auto whole = static_cast<double>(total);
    return (part / whole) * 100.0;
}

static std::string toSizeString(int64_t numWords)
{
    auto totalNum = numWords * sizeof(uint32_t);

    if(totalNum > (8ll * 1024ll * 1024ll * 1024ll))
        return std::to_string(totalNum / (1024ll * 1024ll * 1024ll)) + "GB";
    if(totalNum > (8ll * 1024ll * 1024ll))
        return std::to_string(totalNum / (1024ll * 1024ll)) + "MB";
    if(totalNum > (8ll * 1024ll))
        return std::to_string(totalNum / 1024ll) + "KB";
    return std::to_string(totalNum) + "B";
}

using Histogram = std::array<std::array<char, 60>, 25>;

static void insertColumn(Histogram& histogram, int64_t total, int64_t exec, int64_t tmu, int64_t vpm)
{
    // shift histogram to the left
    for(auto& row : histogram)
    {
        for(std::size_t i = 0; i < row.size() - 1; ++i)
        {
            row[i] = row[i + 1];
        }
    }
    // insert last column
    auto execHeight = toWidth(exec, total, histogram.size());
    auto tmuHeight = toWidth(tmu, total, histogram.size());
    auto vpmHeight = toWidth(vpm, total, histogram.size());
    auto emptyHeight = histogram.size() - execHeight - tmuHeight - vpmHeight;

    for(std::size_t i = 0; i < emptyHeight; ++i)
    {
        histogram[i][histogram[i].size() - 1] = ' ';
    }
    for(std::size_t i = emptyHeight; i < emptyHeight + vpmHeight; ++i)
    {
        histogram[i][histogram[i].size() - 1] = vpmStallChar;
    }
    for(std::size_t i = emptyHeight + vpmHeight; i < emptyHeight + vpmHeight + tmuHeight; ++i)
    {
        histogram[i][histogram[i].size() - 1] = tmuStallChar;
    }
    for(std::size_t i = emptyHeight + vpmHeight + tmuHeight; i < emptyHeight + vpmHeight + tmuHeight + execHeight; ++i)
    {
        histogram[i][histogram[i].size() - 1] = execChar;
    }
}

static void drawHistogram(WINDOW* window, Histogram& histogram, unsigned y, unsigned x)
{
    // TODO draw "nicer"!
    for(std::size_t i = 0; i < histogram.size(); ++i)
    {
        std::string tmp(histogram[i].data(), histogram[i].size());
        // wrapping in string required for trailing 0-byte
        mvwprintw(window, y + i, x, tmp.data());
    }
}

int main(int argc, char** argv)
{
    if(argc > 1 && std::any_of(argv, argv + argc, [](char* arg) -> bool {
           return std::string("--help") == arg || std::string("-h") == arg;
       }))
    {
        std::cout << "Usage: " << std::endl;
        std::cout << std::setw(15) << "--help, -h" << std::setw(4) << " "
                  << "Print this help message and exit" << std::endl;
        std::cout << std::setw(15) << "--noinit" << std::setw(4) << " "
                  << "Disable initialization of the V3D hardware. This can be used to check whether the V3D hardware "
                     "is running"
                  << std::endl;
        return 0;
    }
    // by default (all "older" models) this is 250MHz
    unsigned clockRate = 250000000;
    if(std::none_of(argv, argv + argc, [](char* arg) -> bool { return std::string("--noinit") == arg; }))
    {
        // initializes the mailbox singleton and with it the V3D hardware
        mailbox();
        // if we initialize the mailbox, we can also query the clock rate
        QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> msg({static_cast<uint32_t>(VC4Clock::V3D)});
        if(mailbox()->readMailboxMessage(msg))
        {
            clockRate = msg.getContent(1);
        }
    }
    // initialize ncurses
    auto window = initscr(); // Initialize the window
    noecho();                // Don't echo any keypresses
    curs_set(false);         // Don't display a cursor
    nodelay(window, true);   // Set getting key input non-blocking

    // initialize and reset the counters
    checkResult(V3D::instance()->setCounter(COUNTER_IDLE, CounterType::IDLE_CYCLES));
    checkResult(V3D::instance()->setCounter(COUNTER_EXECUTIONS, CounterType::EXECUTION_CYCLES));
    checkResult(V3D::instance()->setCounter(COUNTER_TMU_STALLS, CounterType::TMU_STALL_CYCLES));
    checkResult(V3D::instance()->setCounter(COUNTER_INSTRUCTION_CACHE_HITS, CounterType::INSTRUCTION_CACHE_HITS));
    checkResult(V3D::instance()->setCounter(COUNTER_INSTRUCTION_CACHE_MISSES, CounterType::INSTRUCTION_CACHE_MISSES));
    checkResult(V3D::instance()->setCounter(COUNTER_UNIFORM_CACHE_HITS, CounterType::UNIFORM_CACHE_HITS));
    checkResult(V3D::instance()->setCounter(COUNTER_UNIFORM_CACHE_MISSES, CounterType::UNIFORM_CACHE_MISSES));
    checkResult(V3D::instance()->setCounter(COUNTER_VPW_STALLS, CounterType::VDW_STALL_CYCES));
    checkResult(V3D::instance()->setCounter(COUNTER_VPR_STALLS, CounterType::VCD_STALL_CYCLES));
    checkResult(V3D::instance()->setCounter(COUNTER_L2_HITS, CounterType::L2_CACHE_HITS));
    checkResult(V3D::instance()->setCounter(COUNTER_L2_MISSES, CounterType::L2_CACHE_MISSES));
    checkResult(V3D::instance()->setCounter(COUNTER_TMU_TOTAL_LOADS, CounterType::TMU_TOTAL_WORDS));
    checkResult(V3D::instance()->setCounter(COUNTER_TMU_CACHE_MISSES, CounterType::TMU_CACHE_MISSES));

    V3D::instance()->resetCounterValue(COUNTER_IDLE);
    V3D::instance()->resetCounterValue(COUNTER_EXECUTIONS);
    V3D::instance()->resetCounterValue(COUNTER_TMU_STALLS);
    V3D::instance()->resetCounterValue(COUNTER_INSTRUCTION_CACHE_HITS);
    V3D::instance()->resetCounterValue(COUNTER_INSTRUCTION_CACHE_MISSES);
    V3D::instance()->resetCounterValue(COUNTER_UNIFORM_CACHE_HITS);
    V3D::instance()->resetCounterValue(COUNTER_UNIFORM_CACHE_MISSES);
    V3D::instance()->resetCounterValue(COUNTER_VPW_STALLS);
    V3D::instance()->resetCounterValue(COUNTER_VPR_STALLS);
    V3D::instance()->resetCounterValue(COUNTER_L2_HITS);
    V3D::instance()->resetCounterValue(COUNTER_L2_MISSES);
    V3D::instance()->resetCounterValue(COUNTER_TMU_TOTAL_LOADS);
    V3D::instance()->resetCounterValue(COUNTER_TMU_CACHE_MISSES);

    std::array<int64_t, 13> counters{};
    counters.fill(0);

    constexpr int barWidth = 50;
    constexpr int barOffset = 10;
    constexpr int cacheOffset = 25;

    std::array<char, barWidth> execBar;
    execBar.fill(execChar);
    std::array<char, barWidth> tmuStallBar;
    tmuStallBar.fill(tmuStallChar);
    std::array<char, barWidth> vpmStallBar;
    vpmStallBar.fill(vpmStallChar);

    Histogram histogram;
    std::for_each(histogram.begin(), histogram.end(), [](std::array<char, 60>& row) { row.fill(' '); });

    // values over whole program execution
    int64_t overallIdleCycles = 0;
    int64_t overallExecCycles = 0;
    int64_t overallTMUStallCycles = 0;
    int64_t overallVPMStallCycles = 0;
    int64_t overallTMULoads = 0;
    auto startTime = std::chrono::high_resolution_clock::now();

    while(true)
    {
        auto loopStartTime = std::chrono::high_resolution_clock::now();
        if(wgetch(window) == 'q')
            break;

        clear(); // clear screen

        // calculate all the values
        std::array<int64_t, counters.size()> newCounters{};
        newCounters.fill(0);
        for(unsigned i = 0; i < counters.size(); ++i)
            newCounters[i] = V3D::instance()->getCounter(i);

        if(std::any_of(newCounters.begin(), newCounters.end(), [](int64_t i) -> bool { return i < 0; }))
        {
            mvwprintw(window, 0, 0, "V3D hardware is not enabled...");
        }
        else
        {
            auto numExecCycles = newCounters[COUNTER_EXECUTIONS] - counters[COUNTER_EXECUTIONS];
            auto numIdleCycles = newCounters[COUNTER_IDLE] - counters[COUNTER_IDLE];
            auto numTMUStallCycles = newCounters[COUNTER_TMU_STALLS] - counters[COUNTER_TMU_STALLS];
            auto numVPMStallCycles = newCounters[COUNTER_VPR_STALLS] + newCounters[COUNTER_VPW_STALLS] -
                counters[COUNTER_VPR_STALLS] - counters[COUNTER_VPW_STALLS];
            auto numUsedCycles = numExecCycles + numTMUStallCycles + numVPMStallCycles;
            auto numTotalCycles = numUsedCycles + numIdleCycles;
            auto numTMULoads = newCounters[COUNTER_TMU_TOTAL_LOADS] - counters[COUNTER_TMU_TOTAL_LOADS];

            auto totalExecCycles = newCounters[COUNTER_EXECUTIONS];
            auto totalIdleCycles = newCounters[COUNTER_IDLE];
            auto totalTMUStallCycles = newCounters[COUNTER_TMU_STALLS];
            auto totalVPMStallCycles = newCounters[COUNTER_VPR_STALLS] + newCounters[COUNTER_VPW_STALLS];
            auto totalUsedCycles = totalExecCycles + totalTMUStallCycles + totalVPMStallCycles;
            auto totalCycles = totalUsedCycles + totalIdleCycles;

            auto totalUCacheHits = newCounters[COUNTER_UNIFORM_CACHE_HITS];
            auto totalUCacheMisses = newCounters[COUNTER_UNIFORM_CACHE_MISSES];
            auto totalICacheHits = newCounters[COUNTER_INSTRUCTION_CACHE_HITS];
            auto totalICacheMisses = newCounters[COUNTER_INSTRUCTION_CACHE_MISSES];
            auto totalL2Hits = newCounters[COUNTER_L2_HITS];
            auto totalL2Misses = newCounters[COUNTER_L2_MISSES];
            auto totalTMULoads = newCounters[COUNTER_TMU_TOTAL_LOADS];
            auto totalTMUCacheMisses = newCounters[COUNTER_TMU_CACHE_MISSES];

            overallIdleCycles += numIdleCycles;
            overallExecCycles += numExecCycles;
            overallTMUStallCycles += numTMUStallCycles;
            overallVPMStallCycles += numVPMStallCycles;
            overallTMULoads += numTMULoads;

            /*
             * Display all the values:
             * +-----------------------------------------------------------
             * |Current:
             * | Total    [==========***~~                              ] (17%)
             * | Details  [============================***********~~~~~~] (60%)
             * |
             * |Overall:
             * | Total    [==========***~~                              ] (17%)
             * | Details  [============================***********~~~~~~] (61%)
             * |
             * |Caches:
             * | Instructions         :   144 (130/14)  (90%/10%)
             * | Parameter (Uniforms) :    12   (11/1)  (99%/1%)
             * | L2                   :  1111 (999/112) (89%/11%)
             * | TMU                  :  1234 (1000/234)(81%/19%) (8000B/s)
             * |
             * |Errors:
             * | list of errors
             * |
             * |Histogram:
             * | [...]
             * |
             * |Legend:
             * | = : QPU execution
             * | * : Input stall (TMU)
             * | ~ : I/O stall (VPM)
             * | cache lines : #total lookups (#hits/#misses) (hits%/misses%)
             * +-------------------------------------------------------------
             */

            mvwprintw(window, 0, 0, "Current:");
            /* print total bar */
            {
                auto execWidth = toWidth(numExecCycles, numTotalCycles, barWidth);
                auto tmuWidth = toWidth(numTMUStallCycles, numTotalCycles, barWidth);
                auto vpmWidth = toWidth(numVPMStallCycles, numTotalCycles, barWidth);
                mvwprintw(window, 1, 1, "Total");
                mvwprintw(window, 1, barOffset, "[");
                mvwprintw(window, 1, barOffset + 1, "%.*s", execWidth, execBar.data());
                mvwprintw(window, 1, barOffset + 1 + execWidth, "%.*s", tmuWidth, tmuStallBar.data());
                mvwprintw(window, 1, barOffset + 1 + execWidth + tmuWidth, "%.*s", vpmWidth, vpmStallBar.data());
                mvwprintw(
                    window, 1, barOffset + 1 + barWidth, "] (%2d%%)", toWidth(numUsedCycles, numTotalCycles, 100));
            }
            /* print details bar */
            {
                auto execWidth = toWidth(numExecCycles, numUsedCycles, barWidth);
                auto tmuWidth = toWidth(numTMUStallCycles, numUsedCycles, barWidth);
                auto vpmWidth = toWidth(numVPMStallCycles, numUsedCycles, barWidth);
                mvwprintw(window, 2, 1, "Details");
                mvwprintw(window, 2, barOffset, "[");
                mvwprintw(window, 2, barOffset + 1, "%.*s", execWidth, execBar.data());
                mvwprintw(window, 2, barOffset + 1 + execWidth, "%.*s", tmuWidth, tmuStallBar.data());
                mvwprintw(window, 2, barOffset + 1 + execWidth + tmuWidth, "%.*s", vpmWidth, vpmStallBar.data());
                mvwprintw(window, 2, barOffset + 1 + barWidth, "] (%2d%%)", toWidth(numExecCycles, numUsedCycles, 100));
            }

            mvwprintw(window, 4, 0, "Overall:");
            /* print total bar */
            {
                // FIXME jumps too much around, should be more or less stable after a few iterations!
                auto execWidth = toWidth(totalExecCycles, totalCycles, barWidth);
                auto tmuWidth = toWidth(totalTMUStallCycles, totalCycles, barWidth);
                auto vpmWidth = toWidth(totalVPMStallCycles, totalCycles, barWidth);
                mvwprintw(window, 5, 1, "Total");
                mvwprintw(window, 5, barOffset, "[");
                mvwprintw(window, 5, barOffset + 1, "%.*s", execWidth, execBar.data());
                mvwprintw(window, 5, barOffset + 1 + execWidth, "%.*s", tmuWidth, tmuStallBar.data());
                mvwprintw(window, 5, barOffset + 1 + execWidth + tmuWidth, "%.*s", vpmWidth, vpmStallBar.data());
                mvwprintw(window, 5, barOffset + 1 + barWidth, "] (%2d%%)", toWidth(totalUsedCycles, totalCycles, 100));
            }
            /* print details bar */
            {
                auto execWidth = toWidth(totalExecCycles, totalUsedCycles, barWidth);
                auto tmuWidth = toWidth(totalTMUStallCycles, totalUsedCycles, barWidth);
                auto vpmWidth = toWidth(totalVPMStallCycles, totalUsedCycles, barWidth);
                mvwprintw(window, 6, 1, "Details");
                mvwprintw(window, 6, barOffset, "[");
                mvwprintw(window, 6, barOffset + 1, "%.*s", execWidth, execBar.data());
                mvwprintw(window, 6, barOffset + 1 + execWidth, "%.*s", tmuWidth, tmuStallBar.data());
                mvwprintw(window, 6, barOffset + 1 + execWidth + tmuWidth, "%.*s", vpmWidth, vpmStallBar.data());
                mvwprintw(
                    window, 6, barOffset + 1 + barWidth, "] (%2d%%)", toWidth(totalExecCycles, totalUsedCycles, 100));
            }

            mvwprintw(window, 8, 0, "Caches:");
            {
                mvwprintw(window, 9, 1, "Instructions         :");
                mvwprintw(window, 9, cacheOffset, "%6" PRId64 " (%6" PRId64 "/%6" PRId64 ") (%2d%%/%2d%%)",
                    totalICacheHits + totalICacheMisses, totalICacheHits, totalICacheMisses,
                    toWidth(totalICacheHits, totalICacheHits + totalICacheMisses, 100),
                    toWidth(totalICacheMisses, totalICacheHits + totalICacheMisses, 100));
                mvwprintw(window, 10, 1, "Parameter (Uniforms) :");
                mvwprintw(window, 10, cacheOffset, "%6" PRId64 " (%6" PRId64 "/%6" PRId64 ") (%2d%%/%2d%%)",
                    totalUCacheHits + totalUCacheMisses, totalUCacheHits, totalUCacheMisses,
                    toWidth(totalUCacheHits, totalUCacheHits + totalUCacheMisses, 100),
                    toWidth(totalUCacheMisses, totalUCacheHits + totalUCacheMisses, 100));
                mvwprintw(window, 11, 1, "L2 cache             :");
                mvwprintw(window, 11, cacheOffset, "%6" PRId64 " (%6" PRId64 "/%6" PRId64 ") (%2d%%/%2d%%)",
                    totalL2Hits + totalL2Misses, totalL2Hits, totalL2Misses,
                    toWidth(totalL2Hits, totalL2Hits + totalL2Misses, 100),
                    toWidth(totalL2Misses, totalL2Hits + totalL2Misses, 100));
                mvwprintw(window, 12, 1, "TMU                  :");
                mvwprintw(window, 12, cacheOffset, "%6" PRId64 " (%6" PRId64 "/%6" PRId64 ") (%2d%%/%2d%%)",
                    totalTMULoads, totalTMULoads - totalTMUCacheMisses, totalTMUCacheMisses,
                    toWidth(totalTMULoads - totalTMUCacheMisses, totalTMULoads, 100),
                    toWidth(totalTMUCacheMisses, totalTMULoads, 100));
                mvwprintw(
                    window, 12, cacheOffset + 42, " (%6s/s)", toSizeString(numTMULoads * intervalsPerSecond).data());
            }

            mvwprintw(window, 14, 0, "Histogram:");
            {
                // TODO use total cycles? but too little usage to display anything
                insertColumn(histogram, numExecCycles + numTMUStallCycles + numVPMStallCycles, numExecCycles,
                    numTMUStallCycles, numVPMStallCycles);
                drawHistogram(window, histogram, 15, 0);
                // TODO only move 1 column per second (half speed)?
            }

            mvwprintw(window, 42, 0, "Errors:");
            mvwprintw(window, 43, 1, getErrors().data());

            mvwprintw(window, 44, 0, "Legend:");
            mvwprintw(window, 45, 1, "= : QPU execution");
            mvwprintw(window, 46, 1, "* : Input stall (TMU)");
            mvwprintw(window, 47, 1, "~ : I/O stall (VPM)");
            mvwprintw(window, 48, 1, "cache lines : #total lookups (#hits/#misses) (hits%%/misses%%)");
        }

        refresh(); // print buffer to terminal/screen

        counters = newCounters;

        // using this guarantees a more stable iteration duration
        std::this_thread::sleep_until(loopStartTime + pollInterval);
    }

    // uninitialize ncurses
    endwin(); // Restore normal terminal behavior

    // on end output summary (e.g. overall usage over all cycles) in text form
    auto endTime = std::chrono::high_resolution_clock::now();
    auto overallTotalCycles = overallExecCycles + overallIdleCycles + overallTMUStallCycles + overallVPMStallCycles;
    // theoretical possible cycles in the elapsed time with 12 QPUs and a clock rate of 250MHz
    auto timespan = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto maxCycles = 12 * clockRate / 1000000 * timespan.count();
    std::cout << "Total cycles:     " << std::setw(20) << overallTotalCycles << " (" << std::setw(5) << std::fixed
              << std::setprecision(2) << toPercent(overallTotalCycles, maxCycles) << "%)" << std::endl;
    std::cout << "Execution cycles: " << std::setw(20) << overallExecCycles << " (" << std::setw(5) << std::fixed
              << std::setprecision(2) << toPercent(overallExecCycles, overallTotalCycles) << "%)" << std::endl;
    std::cout << "TMU stall cycles: " << std::setw(20) << overallTMUStallCycles << " (" << std::setw(5) << std::fixed
              << std::setprecision(2) << toPercent(overallTMUStallCycles, overallTotalCycles) << "%)" << std::endl;
    std::cout << "VPM stall cycles: " << std::setw(20) << overallVPMStallCycles << " (" << std::setw(5) << std::fixed
              << std::setprecision(2) << toPercent(overallVPMStallCycles, overallTotalCycles) << "%)" << std::endl;
    std::cout << "TMU bytes loaded: " << std::setw(20) << (overallTMULoads * sizeof(unsigned)) << " ("
              << toSizeString(overallTMULoads / std::chrono::duration_cast<std::chrono::seconds>(timespan).count())
              << "/s)" << std::endl;

    return 0;
}
#endif