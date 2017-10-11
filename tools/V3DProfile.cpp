/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "common.h"

#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>

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

static int getCounter(uint8_t counter)
{
	int val = V3D::instance().getCounter(counter);
	V3D::instance().resetCounterValue(counter);
	return val;
}

int main(int argc, char** argv)
{
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

	std::size_t width = 12;

	std::cout << std::setw(width) << "QPUs idle" << "|" << std::setw(width) << "QPU exec" << "|" << std::setw(width) << "QPU (%)" << "|" << std::setw(width) << "TMU stalls" << "|"
			<< std::setw(width) << "ICache hits" << "|" << std::setw(width) << "ICache miss" << "|" << std::setw(width) << "UCache hits" << "|" << std::setw(width) << "UCache miss" << "|"
			<< std::setw(width) << "VPW stalls" << "|" << std::setw(width) << "VCD stalls" << "|" << std::setw(width) << "L2 hits" << "|" << std::setw(width) << "L2 miss" << "|"
			<< std::setw(width) << "Errors"
			<< std::endl;

	while(true)
	{
		int qpuIdle = getCounter(COUNTER_IDLE);
		int qpuExec = getCounter(COUNTER_EXECUTIONS);
		float qpuUsed = (qpuIdle < 0 || qpuExec < 0) ? -1.0f : (qpuIdle + qpuExec == 0) ? 0.0f : (qpuExec * 100.0f / (float)(qpuIdle + qpuExec));
		std::cout << std::setw(width) << qpuIdle << "|" << std::setw(width) << qpuExec << "|" << std::setw(width) << qpuUsed << "|"
				<< std::setw(width) << getCounter(COUNTER_TMU_STALLS) << "|" << std::setw(width) << getCounter(COUNTER_INSTRUCTION_CACHE_HITS) << "|"
				<< std::setw(width) << getCounter(COUNTER_INSTRUCTION_CACHE_MISSES) << "|" << std::setw(width) << getCounter(COUNTER_UNIFORM_CACHE_HITS) << "|"
				<< std::setw(width) << getCounter(COUNTER_UNIFORM_CACHE_MISSES) << "|" << std::setw(width) << getCounter(COUNTER_VPW_STALLS) << "|"
				<< std::setw(width) << getCounter(COUNTER_VPR_STALLS) << "|" << std::setw(width) << getCounter(COUNTER_L2_HITS) << "|"
				<< std::setw(width) << getCounter(COUNTER_L2_MISSES) << "|" << std::setw(width) << getErrors() << std::endl;

		std::this_thread::sleep_for(std::chrono::milliseconds{500});
	}

	return 0;
}
