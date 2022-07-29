/*****************************************************************//**
 * \file   source_sdk.h
 * \brief  Header to connect and initialize the SDK subsystems.
 *			Also provides access to core containers (string and buffer)
 *
 * \author Tom <intrinsic.dev@outlook.com>
 * \date   July 2022
 *********************************************************************/
#ifndef _SOURCE_SDK_H_
#define _SOURCE_SDK_H_

#ifdef _WIN32
#pragma once
#endif

#include <Windows.h>
#include <tier0/platform.h>
#include <utlbuffer.h>
#include <utlstring.h>

 /**
  * Interface to allow abstraction of file output methods.
  * Files may be written to a CUtlBuffer or a filestream
  */
abstract_class IWriteStream
{
public:
	virtual void Put(const void* pMem, int size) = 0;
	virtual unsigned int Tell(void) = 0;
};

/**
 * Wrapper to Win32 file API.
 */
class CWin32File
{
public:
	static HANDLE CreateTempFile(CUtlString& WritePath, CUtlString& FileName)
	{
		char tempFileName[MAX_PATH];
		if (WritePath.IsEmpty())
		{
			// use a safe name in the cwd
			char* pBuffer = tmpnam(NULL);
			if (!pBuffer)
			{
				return INVALID_HANDLE_VALUE;
			}
			if (pBuffer[0] == '\\')
			{
				pBuffer++;
			}
			if (pBuffer[strlen(pBuffer) - 1] == '.')
			{
				pBuffer[strlen(pBuffer) - 1] = '\0';
			}
			V_snprintf(tempFileName, sizeof(tempFileName), "_%s.tmp", pBuffer);
		}
		else
		{
			// generate safe name at the desired prefix
			char uniqueFilename[MAX_PATH];
			SYSTEMTIME sysTime;                                                       \
				GetLocalTime(&sysTime);
			sprintf(uniqueFilename, "%d_%d_%d_%d_%d.tmp", sysTime.wDay, sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);                                                \
				V_ComposeFileName(WritePath.String(), uniqueFilename, tempFileName, sizeof(tempFileName));
		}

		FileName = tempFileName;
		HANDLE hFile = CreateFile(tempFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		return hFile;
	}

	static unsigned int FileSeek(HANDLE hFile, unsigned int distance, DWORD MoveMethod)
	{
		LARGE_INTEGER li;

		li.QuadPart = distance;
		li.LowPart = SetFilePointer(hFile, li.LowPart, &li.HighPart, MoveMethod);
		if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
		{
			li.QuadPart = -1;
		}

		return (unsigned int)li.QuadPart;
	}

	static unsigned int FileTell(HANDLE hFile)
	{
		return FileSeek(hFile, 0, FILE_CURRENT);
	}

	static bool FileRead(HANDLE hFile, void* pBuffer, unsigned int size)
	{
		DWORD numBytesRead;
		BOOL bSuccess = ::ReadFile(hFile, pBuffer, size, &numBytesRead, NULL);
		return bSuccess && (numBytesRead == size);
	}

	static bool FileWrite(HANDLE hFile, void* pBuffer, unsigned int size)
	{
		DWORD numBytesWritten;
		BOOL bSuccess = WriteFile(hFile, pBuffer, size, &numBytesWritten, NULL);
		return bSuccess && (numBytesWritten == size);
	}
};

/**
 * Wrapper for CUtlBuffer methods.
 */
class CBufferStream : public IWriteStream
{
public:
	CBufferStream(CUtlBuffer& buff) : IWriteStream(), m_buff(&buff) {}

	virtual void Put(const void* pMem, int size) { m_buff->Put(pMem, size); }
	virtual unsigned int Tell(void) { return m_buff->TellPut(); }

private:
	CUtlBuffer* m_buff;
};

/**
 * Wrapper for file I/O methods.
 */
class CFileStream : public IWriteStream
{
public:
	CFileStream(FILE* fout) : IWriteStream(), m_file(fout), m_hFile(INVALID_HANDLE_VALUE) {}
	CFileStream(HANDLE hOutFile) : IWriteStream(), m_file(NULL), m_hFile(hOutFile) {}

	virtual void Put(const void* pMem, int size)
	{
		if (m_file)
		{
			fwrite(pMem, size, 1, m_file);
		}
		else
		{
			DWORD numBytesWritten;
			WriteFile(m_hFile, pMem, size, &numBytesWritten, NULL);
		}
	}

	virtual unsigned int Tell(void)
	{
		if (m_file)
		{
			return ftell(m_file);
		}
		else
		{
			return CWin32File::FileTell(m_hFile);
		}
	}

private:
	FILE* m_file;
	HANDLE	m_hFile;
};

#endif // _SOURCE_SDK_H_
