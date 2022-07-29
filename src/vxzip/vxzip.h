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
