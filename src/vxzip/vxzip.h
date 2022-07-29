/*****************************************************************//**
 * \file   vxzip.h
 * \brief  Source Engine Application object for this app
 *
 * \author tom
 * \date   July 2022
 *********************************************************************/
#pragma once

#include <appframework/appframework.h>
#include "xzip_file.h"

 /**
  * Implementation of appsystem interface.
  */
class CVXZipApp : public CDefaultAppSystemGroup< CAppSystemGroup >
{
public:
	/**
	 * Called on construction of the object.
	 *
	 * \return True on success, False on Error
	 */
	virtual bool Create();
	/**
	 * Pre-initialization routine
	 *
	 * \return True on success, False on Error
	 */
	virtual bool PreInit();
	/**
	 * Main routine (monolithic).
	 *
	 * \return True on success, False on Error
	 */
	virtual int Main();
	/**
	 * Cleanup routine.
	 *
	 */
	virtual void PostShutdown();

public:
	/**
	 * Contains the zip file handles held by this application.
	 */
	CUtlLinkedList< HANDLE > m_ZipFileHandles;
};

/**
 * Register the application.
 */
DEFINE_CONSOLE_APPLICATION_OBJECT(CVXZipApp);

/**
 * Redirection of the log messages to signal failures.
 *
 * ex: SPEW_ASSERT will return SPEW_DEBUGGER to log this message to the debugger as well.
 *
 * \param spewType Message spew type
 * \param pMsg
 * \return Spew return value
 */
static SpewRetval_t OutputFunc(SpewType_t spewType, char const* pMsg)
{
	// write to standard output
	printf(pMsg);

	// determine our spew result
	switch (spewType)
	{
	case SPEW_ERROR: return SPEW_ABORT;
	case SPEW_ASSERT:return SPEW_DEBUGGER;
	default: return SPEW_CONTINUE;
	}
}
