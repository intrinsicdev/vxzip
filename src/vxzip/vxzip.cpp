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

	// todo: Add required appsystem module loads here

	return true;
}

bool CVXZipApp::PreInit()
{
	CreateInterfaceFn factory = GetFactory();

	ConnectTier1Libraries(&factory, 1);
	ConnectTier2Libraries(&factory, 1);

	// todo: Null check connected appsystems here. If null return false

	return true;
}

int CVXZipApp::Main()
{
	return 0;
}

void CVXZipApp::PostShutdown()
{
	// post shutdown should do PreInit() but in reverse

	// todo: Dispose of connected appsystems here

	// always disconnect these last and in this order
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
	return;
}