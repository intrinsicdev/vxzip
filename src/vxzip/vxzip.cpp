/*****************************************************************//**
 * \file   vxzip.cpp
 * \brief  Main entry point
 *
 * \author Tom <intrinsicdev@outlook.com>
 * \date   July 2022
 *********************************************************************/
#include "vxzip.h"

bool CVXZipApp::Create()
{
	// Redirect spew output
	SpewOutputFunc(OutputFunc);
	return false;
}

bool CVXZipApp::PreInit()
{
	return false;
}

int CVXZipApp::Main()
{
	return 0;
}

void CVXZipApp::PostShutdown()
{
}