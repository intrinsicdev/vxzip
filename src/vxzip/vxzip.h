/*****************************************************************//**
 * \file   vxzip.h
 * \brief  Source Engine Application object for this app
 *
 * \author Tom <intrinsic.dev@outlook.com>
 * \date   July 2022
 *********************************************************************/
#pragma once
#include <appframework/appframework.h>
#include <tier0/icommandline.h>
#include <tier1/tier1.h>
#include <tier2/tier2.h>
#include <../../utils/common/scriplib.h>
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
	 * Spews app help and usage.
	 *
	 */
	void PrintHelp();
	/**
	 * Parses the commandline options into the application object.
	 *
	 * \return True indicates success
	 */
	bool ParseCommandLine();
	/**
	 * Extracts an xzip pak file to the file system.
	 *
	 * \param outputPath	Output directory pak files
	 * \param zipPath		Input path for xzip pak
	 * \return True indicates success
	 */
	void ExtractXZip(CUtlString& outputPath, CUtlString& zipPath);
	/**
	 * Constructs an xzip pak from a given directory.
	 *
	 * \param inputPath		Input directory to pack
	 * \param zipPath		Output path for xzip pak
	 * \return True indicates success
	 */
	void BuildXZip(CUtlString& inputPath, CUtlString& zipPath);

private:
	// parameter tokens
	const char* m_szTargetToken = "-t";
	const char* m_szExtractToken = "-e";
	const char* m_szBuildToken = "-b";

	/**
	 * Opens an XZip pak file for reading.
	 *
	 * \param inputPath
	 * \return
	 */
	void OpenXZip(CUtlString& inputPath);
	/**
	 * Writes the current pak file to the disk.
	 *
	 * \param outputPath
	 * \return
	 */
	void SaveXZip(CUtlString& outputPath, bool bClose = false);

	/**
	 * Closes the XZip pak file (deallocation).
	 *
	 */
	void CloseXZip();

	void ExtractAllFiles(CUtlString* outputPath);
	bool ExtractFile(CUtlSymbol* fileSymbol, int fileSize, CUtlString* outputPath);

	/**
	 * Object pointer to CXZip for this instance.
	 */
	CXZipFile* m_pXZipFile;
	/**
	 * Win32 handle to the XZip file.
	 */
	HANDLE m_hXZipFile;
};

/**
 * Register the application.
 */
DEFINE_CONSOLE_APPLICATION_OBJECT(CVXZipApp);
