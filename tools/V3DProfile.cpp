/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

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

int main(int argc, char** argv)
{
    // initialize ncurses
    auto window = initscr(); // Initialize the window
    noecho();                // Don't echo any keypresses
    curs_set(false);         // Don't display a cursor
    nodelay(window, 1);      // Set getting key input non-blocking

    checkResult(V3D::instance().setCounter(COUNTER_IDLE, CounterType::IDLE_CYCLES));
    checkResult(V3D::instance().setCounter(COUNTER_EXECUTIONS, CounterType::EXECUTION_CYCLES));
    checkResult(V3D::instance().setCounter(COUNTER_TMU_STALLS, CounterType::TMU_STALL_CYCLES));
    checkResult(V3D::instance().setCounter(COUNTER_INSTRUCTION_CACHE_HITS, CounterType::INSTRUCTION_CACHE_HITS));
    checkResult(V3D::instance().setCounter(COUNTER_INSTRUCTION_CACHE_MISSES, CounterType::INSTRUCTION_CACHE_MISSES));
    checkResult(V3D::instance().setCounter(COUNTER_UNIFORM_CACHE_HITS, CounterType::UNIFORM_CACHE_HITS));
    checkResult(V3D::instance().setCounter(COUNTER_UNIFORM_CACHE_MISSES, CounterType::UNIFORM_CACHE_MISSES));
    checkResult(V3D::instance().setCounter(COUNTER_VPW_STALLS, CounterType::VPW_STALL_CYCES));
    checkResult(V3D::instance().setCounter(COUNTER_VPR_STALLS, CounterType::VCD_STALL_CYCLES));
    checkResult(V3D::instance().setCounter(COUNTER_L2_HITS, CounterType::L2_CACHE_HITS));
    checkResult(V3D::instance().setCounter(COUNTER_L2_MISSES, CounterType::L2_CACHE_MISSES));

    std::array<int64_t, 11> counters{};
    counters.fill(0);

    constexpr int barWidth = 50;
    constexpr int barOffset = 10;
    constexpr int cacheOffset = 25;

    constexpr char execChar = '=';
    constexpr char tmuStallChar = '*';
    constexpr char vpmStallChar = '~';

    std::array<char, barWidth> execBar;
    execBar.fill(execChar);
    std::array<char, barWidth> tmuStallBar;
    tmuStallBar.fill(tmuStallChar);
    std::array<char, barWidth> vpmStallBar;
    vpmStallBar.fill(vpmStallChar);

    while(true)
    {
        if(wgetch(window) == 'q')
            break;

        clear(); // clear screen

        // calculate all the values
        std::array<int64_t, counters.size()> newCounters{};
        newCounters.fill(0);
        for(unsigned i = 0; i < counters.size(); ++i)
            newCounters[i] = V3D::instance().getCounter(i);

        if(std::any_of(
               newCounters.begin(), newCounters.end(), [](int64_t i) -> bool { return i < 0; }))
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
             * |
             * |Errors:
             * | list of errors
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
            }

            mvwprintw(window, 13, 0, "Errors:");
            mvwprintw(window, 14, 1, getErrors().data());

            mvwprintw(window, 15, 0, "Legend:");
            mvwprintw(window, 16, 1, "= : QPU execution");
            mvwprintw(window, 17, 1, "* : Input stall (TMU)");
            mvwprintw(window, 18, 1, "~ : I/O stall (VPM)");
            mvwprintw(window, 19, 1, "cache lines : #total lookups (#hits/#misses) (hits%%/misses%%)");
        }

        refresh(); // print buffer to terminal/screen

        counters = newCounters;

        std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }

    // uninitialize ncurses
    endwin(); // Restore normal terminal behavior

    return 0;
}
#endif