/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TOOLS_COMMON_H_
#define TOOLS_COMMON_H_

#include "hal/V3D.h"

std::string getErrors(const vc4cl::V3D& v3d)
{
    using namespace vc4cl;
    std::string res;
    if(v3d.hasError(ErrorType::VCD_OOS))
        res += "VCD_OOS";
    if(v3d.hasError(ErrorType::VDW_OVERFLOW))
        res += " VDW_OF";
    if(v3d.hasError(ErrorType::VPM_SIZE_ERROR))
        res += " VPM_SIZE";
    if(v3d.hasError(ErrorType::VPM_FREE_NONALLOCATED))
        res += " VPM_FNA";
    if(v3d.hasError(ErrorType::VPM_WRITE_NONALLOCATED))
        res += " VPM_WNA";
    if(v3d.hasError(ErrorType::VPM_READ_NONALLOCATED))
        res += " VPM_RNA";
    if(v3d.hasError(ErrorType::VPM_READ_RANGE))
        res += " VPM_RR";
    if(v3d.hasError(ErrorType::VPM_WRITE_RANGE))
        res += " VPM_WR";
    if(v3d.hasError(ErrorType::VPM_REQUEST_TOO_BIG))
        res += " VPM_REQ";
    if(v3d.hasError(ErrorType::VPM_ALLOCATING_WHILE_BUSY))
        res += " VPM_BUSY";
    return res;
}

#endif /* TOOLS_COMMON_H_ */
