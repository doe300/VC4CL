/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TOOLS_COMMON_H_
#define TOOLS_COMMON_H_

#include "V3D.h"

std::string getErrors()
{
	using namespace vc4cl;
	std::string res;
	if(V3D::instance().hasError(ErrorType::VCD_OOS))
		res += "VCD_OOS";
	if(V3D::instance().hasError(ErrorType::VDW_OVERFLOW))
		res += " VDW_OF";
	if(V3D::instance().hasError(ErrorType::VPM_SIZE_ERROR))
		res += " VPM_SIZE";
	if(V3D::instance().hasError(ErrorType::VPM_FREE_NONALLOCATED))
		res += " VPM_FNA";
	if(V3D::instance().hasError(ErrorType::VPM_WRITE_NONALLOCATED))
		res += " VPM_WNA";
	if(V3D::instance().hasError(ErrorType::VPM_READ_NONALLOCATED))
		res += " VPM_RNA";
	if(V3D::instance().hasError(ErrorType::VPM_READ_RANGE))
		res += " VPM_RR";
	if(V3D::instance().hasError(ErrorType::VPM_WRITE_RANGE))
		res += " VPM_WR";
	if(V3D::instance().hasError(ErrorType::VPM_REQUEST_TOO_BIG))
		res += " VPM_REQ";
	if(V3D::instance().hasError(ErrorType::VPM_ALLOCATING_WHILE_BUSY))
		res += " VPM_BUSY";
	return res;
}


#endif /* TOOLS_COMMON_H_ */
