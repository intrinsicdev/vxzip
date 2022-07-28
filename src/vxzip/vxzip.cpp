/*****************************************************************//**
 * \file   vxzip.cpp
 * \brief  Main entry point
 * 
 * \author Tom <intrinsicdev@outlook.com>
 * \date   July 2022
 *********************************************************************/
#include <memory>
#include "xzip_file.h"

int main()
{
	auto pZipFile = new CXZipFile("D:\\temp\\vxzip\\tmp", true);
	auto hZipFile = pZipFile->OpenFromDisk("D:\\temp\\vxzip\\test.360.zip");

	if (hZipFile != INVALID_HANDLE_VALUE) 
		pZipFile->SpewDirectory();
	else 
		printf("fail\n");

	free(pZipFile);

	return 0;
}