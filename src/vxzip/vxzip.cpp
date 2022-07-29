/*****************************************************************//**
 * \file   vxzip.cpp
 * \brief  Main entry point
 *
 * \author Tom <intrinsic.dev@outlook.com>
 * \date   July 2022
 *********************************************************************/
#include "vxzip.h"

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

	switch (spewType)
	{
	case SPEW_ERROR: return SPEW_ABORT;
	case SPEW_ASSERT:return SPEW_DEBUGGER;
	default: return SPEW_CONTINUE;
	}
}

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
	if (!ParseCommandLine())
	{
		PrintHelp();
		return 1;
	}

	auto idxBuildParam = CommandLine()->FindParm(this->m_szBuildToken);
	auto idxExtractParam = CommandLine()->FindParm(this->m_szExtractToken);
	auto idxTargetParam = CommandLine()->FindParm(this->m_szTargetToken);

	auto paramTarget = CUtlString(CommandLine()->GetParm(idxTargetParam + 1));
	auto paramAction = CUtlString();

	if (idxBuildParam)
	{
		// build xzip
		paramAction.Set(CommandLine()->GetParm(idxBuildParam + 1));
		this->BuildXZip(paramTarget, paramAction);
	}
	else
	{
		// extract xzip
		paramAction.Set(CommandLine()->GetParm(idxExtractParam + 1));

		this->ExtractXZip(paramAction, paramTarget);
	}

	// success.
	Msg("Done, SUCCESS!\n");
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

void CVXZipApp::PrintHelp()
{
	Msg("vxzip - Valve Source Engine Console Game Asset Builder\n");
	Msg("by Intrinsic <intrinsic.dev@outlook.com>\n(build: %s %s)\n", __DATE__, __TIME__);
	Msg("\n");

	Msg("Usage:\n");
	Msg("\tvxzip.exe [options]\n");

	Msg("\n");
	Msg("Options:\n");
	Msg("\t%s [input folder]            Build pak file(s)\n", this->m_szBuildToken);
	Msg("\t%s [input zip]               Extract pak file\n", this->m_szExtractToken);
	Msg("\t%s [target zip or folder]    Target zip filename or output folder\n", this->m_szTargetToken);
	Msg("\n");
}

bool CVXZipApp::ParseCommandLine()
{
	auto idxBuildArg = CommandLine()->FindParm(this->m_szBuildToken);
	auto idxExtractArg = CommandLine()->FindParm(this->m_szExtractToken);
	auto idxTargetArg = CommandLine()->FindParm(this->m_szTargetToken);

	if (!idxTargetArg ||					// target parameter is always required
		(!idxBuildArg && !idxExtractArg) ||	// we need a build/extract parameter
		(idxBuildArg && idxExtractArg)		// cannot do both
		)
	{
		Error("Invalid parameter(s) provided.\n");
		return false;
	}

	return true;
}

void CVXZipApp::ExtractXZip(CUtlString& outputPath, CUtlString& zipPath)
{
	OpenXZip(zipPath);
	ExtractAllFiles(outputPath);
}

void CVXZipApp::BuildXZip(CUtlString& inputPath, CUtlString& zipPath)
{
	// todo: Build file list from inputPath
	// todo: Convert files that qualify
	// todo: Assemble XZip pak
	// todo: Write XZip pak to disk
}

void CVXZipApp::OpenXZip(CUtlString& inputPath)
{
	m_pXZipFile = new CXZipFile(NULL, true);

	m_hXZipFile = m_pXZipFile->OpenFromDisk(inputPath.Get());
	Assert(m_hXZipFile);
}

void CVXZipApp::SaveXZip(CUtlString& outputPath, bool bClose)
{
	m_pXZipFile->SaveToDisk(m_hXZipFile);

	if (bClose)
		CloseXZip();
}

void CVXZipApp::CloseXZip()
{
	if (m_hXZipFile != INVALID_HANDLE_VALUE)
		m_hXZipFile = INVALID_HANDLE_VALUE;

	if (m_pXZipFile)
		delete m_pXZipFile;
}

void CVXZipApp::ExtractAllFiles(CUtlString& outputPath)
{
	auto iEntryID = -1;
	auto iFileSize = 0;
	CUtlSymbol entrySymbol;

	// get first entry
	iEntryID = m_pXZipFile->GetNextEntry(iEntryID, entrySymbol, iFileSize);

	// walk the directory
	while (iEntryID > -1)
	{
		Msg("Extracting - %s\n", entrySymbol.String());

		// next...
		iEntryID = m_pXZipFile->GetNextEntry(iEntryID, entrySymbol, iFileSize);
	}
}

void CVXZipApp::ExtractFile(CUtlSymbol& fileSymbol, CUtlString& outputPath)
{
	// todo: Assemble final path on disk
	// todo: Validate/Create final file (default to override)
	//	todo: Add '-f' option for forcing - aka overwrite if exists
	// todo: Extract file to disk 
}