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

	AppSystemInfo_t appSystems[] =
	{
		// todo: Add required appsystem module loads here
		{ "", "" }	// Required to terminate the list
	};
	return AddSystems(appSystems);
}

bool CVXZipApp::PreInit()
{
	CreateInterfaceFn factory = GetFactory();

	ConnectTier1Libraries(&factory, 1);
	ConnectTier2Libraries(&factory, 1);

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
	free(m_hXZipFile);
	free(m_pXZipFile);
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
	fs::path path { outputPath.AbsPath().Get() };
	OpenXZip(zipPath);
	ExtractAllFiles(path);
}

void CVXZipApp::BuildXZip(CUtlString& inputPath, CUtlString& zipPath)
{
	// todo: Build file list from inputPath
	// todo: Convert files that qualify
	// todo: Assemble XZip pak
	// todo: Write XZip pak to disk
}

void CVXZipApp::OpenXZip(const char* pszZipPath)
{
	m_pXZipFile = new CXZipFile(NULL, true);

	m_hXZipFile = m_pXZipFile->OpenFromDisk(pszZipPath);
	Assert(m_hXZipFile);
}

void CVXZipApp::SaveXZip(const fs::path& outputPath, bool bClose)
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


void CVXZipApp::ExtractAllFiles(const fs::path& outputPath)
{
	auto iEntryID = -1;
	auto iFileSize = 0;
	CUtlSymbol entrySymbol;

	// get first entry
	iEntryID = m_pXZipFile->GetNextEntry(iEntryID, entrySymbol, iFileSize);

	// walk the directory
	while (iEntryID > -1)
	{
		// extract file
		if (ExtractFile(entrySymbol.String(), outputPath))
			Msg("Extracted - %s\n", entrySymbol.String());
		else
			Error("Failed to extract - %s\n", entrySymbol.String());

		// next...
		iEntryID = m_pXZipFile->GetNextEntry(iEntryID, entrySymbol, iFileSize);
	}
}

bool CVXZipApp::ExtractFile(const char* pszRelPath, const std::filesystem::path& path)
{
	// create final path
	auto finalPath = (fs::path{ path } /= pszRelPath);
	CUtlBuffer fileBuffer;

	bool bIsText = false;
	auto fileExt = finalPath.extension();

	if (fileExt == ".cfg" || fileExt == ".txt" ||
		fileExt == ".vmt" /*|| fileExt == */ )
	{
		bIsText = true;
	}


	auto fileSize = m_pXZipFile->ReadFile(m_hXZipFile, pszRelPath, bIsText, fileBuffer);

	if (fileBuffer.IsValid())
	{
		if (!(fs::exists(finalPath.parent_path())))
			fs::create_directories(finalPath.parent_path());

		auto hFile = CreateFile(finalPath.string().c_str(), 
			GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		return CWin32File::FileWrite(hFile, fileBuffer.Base(), fileSize);
	}

	return false;
}
