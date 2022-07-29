/*****************************************************************//**
 * \file   xzip_file.cpp
 * \brief  XZip format support. Based on the zip code provided
 *			by the public SDK.
 *
 * \author Tom <intrinsic.dev@outlook.com>
 * \date   July 2022
 *********************************************************************/

#include "xzip_file.h"

 // Data descriptions for byte swapping
BEGIN_BYTESWAP_DATADESC(ZIP_EndOfCentralDirRecord)
DEFINE_FIELD(signature, FIELD_INTEGER),
DEFINE_FIELD(numberOfThisDisk, FIELD_SHORT),
DEFINE_FIELD(numberOfTheDiskWithStartOfCentralDirectory, FIELD_SHORT),
DEFINE_FIELD(nCentralDirectoryEntries_ThisDisk, FIELD_SHORT),
DEFINE_FIELD(nCentralDirectoryEntries_Total, FIELD_SHORT),
DEFINE_FIELD(centralDirectorySize, FIELD_INTEGER),
DEFINE_FIELD(startOfCentralDirOffset, FIELD_INTEGER),
DEFINE_FIELD(commentLength, FIELD_SHORT),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC(ZIP_FileHeader)
DEFINE_FIELD(signature, FIELD_INTEGER),
DEFINE_FIELD(versionMadeBy, FIELD_SHORT),
DEFINE_FIELD(versionNeededToExtract, FIELD_SHORT),
DEFINE_FIELD(flags, FIELD_SHORT),
DEFINE_FIELD(compressionMethod, FIELD_SHORT),
DEFINE_FIELD(lastModifiedTime, FIELD_SHORT),
DEFINE_FIELD(lastModifiedDate, FIELD_SHORT),
DEFINE_FIELD(crc32, FIELD_INTEGER),
DEFINE_FIELD(compressedSize, FIELD_INTEGER),
DEFINE_FIELD(uncompressedSize, FIELD_INTEGER),
DEFINE_FIELD(fileNameLength, FIELD_SHORT),
DEFINE_FIELD(extraFieldLength, FIELD_SHORT),
DEFINE_FIELD(fileCommentLength, FIELD_SHORT),
DEFINE_FIELD(diskNumberStart, FIELD_SHORT),
DEFINE_FIELD(internalFileAttribs, FIELD_SHORT),
DEFINE_FIELD(externalFileAttribs, FIELD_INTEGER),
DEFINE_FIELD(relativeOffsetOfLocalHeader, FIELD_INTEGER),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC(ZIP_LocalFileHeader)
DEFINE_FIELD(signature, FIELD_INTEGER),
DEFINE_FIELD(versionNeededToExtract, FIELD_SHORT),
DEFINE_FIELD(flags, FIELD_SHORT),
DEFINE_FIELD(compressionMethod, FIELD_SHORT),
DEFINE_FIELD(lastModifiedTime, FIELD_SHORT),
DEFINE_FIELD(lastModifiedDate, FIELD_SHORT),
DEFINE_FIELD(crc32, FIELD_INTEGER),
DEFINE_FIELD(compressedSize, FIELD_INTEGER),
DEFINE_FIELD(uncompressedSize, FIELD_INTEGER),
DEFINE_FIELD(fileNameLength, FIELD_SHORT),
DEFINE_FIELD(extraFieldLength, FIELD_SHORT),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC(ZIP_PreloadHeader)
DEFINE_FIELD(Version, FIELD_INTEGER),
DEFINE_FIELD(DirectoryEntries, FIELD_INTEGER),
DEFINE_FIELD(PreloadDirectoryEntries, FIELD_INTEGER),
DEFINE_FIELD(Alignment, FIELD_INTEGER),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC(ZIP_PreloadDirectoryEntry)
DEFINE_FIELD(Length, FIELD_INTEGER),
DEFINE_FIELD(DataOffset, FIELD_INTEGER),
END_BYTESWAP_DATADESC()

/**
 * Copies text data from a form appropriate for disk to a normal string.
 *
 * \param pSrc
 * \param nSrcSize
 * \param buf
 */
	static void ReadTextData(const char* pSrc, int nSrcSize, CUtlBuffer& buf)
{
	buf.EnsureCapacity(nSrcSize + 1);
	const char* pSrcEnd = pSrc + nSrcSize;
	for (const char* pSrcScan = pSrc; pSrcScan < pSrcEnd; ++pSrcScan)
	{
		if (*pSrcScan == '\r')
		{
			if (pSrcScan[1] == '\n')
			{
				buf.PutChar('\n');
				++pSrcScan;
				continue;
			}
		}

		buf.PutChar(*pSrcScan);
	}

	// Null terminate
	buf.PutChar('\0');
}

/**
 * Copies text data into a form appropriate for disk.
 *
 * \param pDst
 * \param pSrc
 * \param dstSize
 * \param srcSize
 */
static void CopyTextData(char* pDst, const char* pSrc, int dstSize, int srcSize)
{
	const char* pSrcScan = pSrc;
	const char* pSrcEnd = pSrc + srcSize;
	char* pDstScan = pDst;

#ifdef DBGFLAG_ASSERT
	char* pDstEnd = pDst + dstSize;
#endif

	for (; pSrcScan < pSrcEnd; pSrcScan++)
	{
		if (*pSrcScan == '\n')
		{
			*pDstScan = '\r';
			pDstScan++;
			*pDstScan = '\n';
			pDstScan++;
		}
		else
		{
			*pDstScan = *pSrcScan;
			pDstScan++;
		}
	}
	Assert(pSrcScan == pSrcEnd);
	Assert(pDstScan == pDstEnd);
}

static int GetLengthOfBinStringAsText(const char* pSrc, int srcSize)
{
	const char* pSrcScan = pSrc;
	const char* pSrcEnd = pSrc + srcSize;
	int numChars = 0;
	for (; pSrcScan < pSrcEnd; pSrcScan++)
	{
		if (*pSrcScan == '\n')
		{
			numChars += 2;
		}
		else
		{
			numChars++;
		}
	}
	return numChars;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CXZipFile::CZipEntry::CZipEntry(void)
{
	m_Name = "";
	m_nCompressedSize = 0;
	m_nUncompressedSize = 0;
	m_pData = NULL;
	m_ZipOffset = 0;
	m_ZipCRC = 0;
	m_DiskCacheOffset = 0;
	m_SourceDiskOffset = 0;
	m_eCompressionType = IZip::eCompressionType_None;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : src -
//-----------------------------------------------------------------------------
CXZipFile::CZipEntry::CZipEntry(const CXZipFile::CZipEntry& src)
{
	m_Name = src.m_Name;
	m_nCompressedSize = src.m_nCompressedSize;
	m_nUncompressedSize = src.m_nUncompressedSize;
	m_eCompressionType = src.m_eCompressionType;

	if (src.m_nCompressedSize > 0 && src.m_pData)
	{
		m_pData = malloc(src.m_nCompressedSize);
		memcpy(m_pData, src.m_pData, src.m_nCompressedSize);
	}
	else
	{
		m_pData = NULL;
	}

	m_ZipOffset = src.m_ZipOffset;
	m_ZipCRC = src.m_ZipCRC;
	m_DiskCacheOffset = src.m_DiskCacheOffset;
	m_SourceDiskOffset = src.m_SourceDiskOffset;
}

//-----------------------------------------------------------------------------
// Purpose: Clear any leftover data
//-----------------------------------------------------------------------------
CXZipFile::CZipEntry::~CZipEntry(void)
{
	if (m_pData)
	{
		free(m_pData);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Construction
//-----------------------------------------------------------------------------
CXZipFile::CXZipFile(const char* pDiskCacheWritePath, bool bSortByName)
	: m_Files(0, 32)
{
	m_AlignmentSize = 0;
	m_bForceAlignment = false;
	m_bCompatibleFormat = true;

	m_bUseDiskCacheForWrites = (pDiskCacheWritePath != NULL);
	m_DiskCacheWritePath = pDiskCacheWritePath;
	m_hDiskCacheWriteFile = INVALID_HANDLE_VALUE;

	if (bSortByName)
	{
		m_Files.SetLessFunc(CZipEntry::ZipFileLessFunc_CaselessSort);
	}
	else
	{
		m_Files.SetLessFunc(CZipEntry::ZipFileLessFunc);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destroy zip data
//-----------------------------------------------------------------------------
CXZipFile::~CXZipFile(void)
{
	m_bUseDiskCacheForWrites = false;
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Delete all current data
//-----------------------------------------------------------------------------
void CXZipFile::Clear(void)
{
	m_Files.RemoveAll();

	if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hDiskCacheWriteFile);
		DeleteFile(m_DiskCacheName.String());
		m_hDiskCacheWriteFile = INVALID_HANDLE_VALUE;
	}

	if (m_bUseDiskCacheForWrites)
	{
		m_hDiskCacheWriteFile = CWin32File::CreateTempFile(m_DiskCacheWritePath, m_DiskCacheName);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Comparison for sorting entries
// Input  : src1 -
//			src2 -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CXZipFile::CZipEntry::ZipFileLessFunc(CZipEntry const& src1, CZipEntry const& src2)
{
	return (src1.m_Name < src2.m_Name);
}

bool CXZipFile::CZipEntry::ZipFileLessFunc_CaselessSort(CZipEntry const& src1, CZipEntry const& src2)
{
	return (V_stricmp(src1.m_Name.String(), src2.m_Name.String()) < 0);
}

void CXZipFile::ForceAlignment(bool bAligned, bool bCompatibleFormat, unsigned int alignment)
{
	m_bForceAlignment = bAligned;
	m_AlignmentSize = alignment;
	m_bCompatibleFormat = bCompatibleFormat;

	if (!bAligned)
	{
		m_AlignmentSize = 0;
	}
	else if (!IsPowerOfTwo(m_AlignmentSize))
	{
		m_AlignmentSize = 0;
	}
}

unsigned int CXZipFile::GetAlignment()
{
	if (!m_bForceAlignment || !m_AlignmentSize)
	{
		return 0;
	}

	return m_AlignmentSize;
}

void CXZipFile::SetBigEndian(bool bigEndian)
{
	m_Swap.SetTargetBigEndian(bigEndian);
}

void CXZipFile::ActivateByteSwapping(bool bActivate)
{
	m_Swap.ActivateByteSwapping(bActivate);
}

//-----------------------------------------------------------------------------
// Purpose: Load pak file from raw buffer
// Input  : *buffer -
//			bufferlength -
//-----------------------------------------------------------------------------
void CXZipFile::OpenFromBuffer(void* buffer, int bufferlength)
{
	// Throw away old data
	Clear();

	// Initialize a buffer
	CUtlBuffer buf(0, bufferlength + 1);					// +1 for null termination

	// need to swap bytes, so set the buffer opposite the machine's endian
	buf.ActivateByteSwapping(m_Swap.IsSwappingBytes());

	buf.Put(buffer, bufferlength);

	buf.SeekGet(CUtlBuffer::SEEK_TAIL, 0);
	unsigned int fileLen = buf.TellGet();

	// Start from beginning
	buf.SeekGet(CUtlBuffer::SEEK_HEAD, 0);

	ZIP_EndOfCentralDirRecord rec = { 0 };

#ifdef DBGFLAG_ASSERT
	bool bFoundEndOfCentralDirRecord = false;
#endif
	unsigned int offset = fileLen - sizeof(ZIP_EndOfCentralDirRecord);
	// If offset is ever greater than startOffset then it means that it has
	// wrapped. This used to be a tautological >= 0 test.
	ANALYZE_SUPPRESS(6293); // warning C6293: Ill-defined for-loop: counts down from minimum
	for (unsigned int startOffset = offset; offset <= startOffset; offset--)
	{
		buf.SeekGet(CUtlBuffer::SEEK_HEAD, offset);
		buf.GetObjects(&rec);
		if (rec.signature == PKID(5, 6))
		{
#ifdef DBGFLAG_ASSERT
			bFoundEndOfCentralDirRecord = true;
#endif

			// Set any xzip configuration
			if (rec.commentLength)
			{
				char commentString[128];
				int commentLength = min(rec.commentLength, sizeof(commentString));
				buf.Get(commentString, commentLength);
				if (commentLength == sizeof(commentString))
					--commentLength;
				commentString[commentLength] = '\0';
				ParseXZipCommentString(commentString);
			}
			break;
		}
		else
		{
			// wrong record
			rec.nCentralDirectoryEntries_Total = 0;
		}
	}
	Assert(bFoundEndOfCentralDirRecord);

	// Make sure there are some files to parse
	int numzipfiles = rec.nCentralDirectoryEntries_Total;
	if (numzipfiles <= 0)
	{
		// No files
		return;
	}

	buf.SeekGet(CUtlBuffer::SEEK_HEAD, rec.startOfCentralDirOffset);

	// Allocate space for directory
	TmpFileInfo_t* newfiles = new TmpFileInfo_t[numzipfiles];
	Assert(newfiles);

	// build directory
	int i;
	for (i = 0; i < rec.nCentralDirectoryEntries_Total; i++)
	{
		ZIP_FileHeader zipFileHeader;
		buf.GetObjects(&zipFileHeader);
		Assert(zipFileHeader.signature == PKID(1, 2));
		if (zipFileHeader.compressionMethod != IZip::eCompressionType_None &&
			zipFileHeader.compressionMethod != IZip::eCompressionType_LZMA)
		{
			Assert(false);
			Warning("Opening ZIP file with unsupported compression type\n");
		}

		char tmpString[1024] = { 0 };
		buf.Get(tmpString, Min((unsigned int)zipFileHeader.fileNameLength, (unsigned int)sizeof(tmpString)));
		Q_strlower(tmpString);

		// can determine actual filepos, assuming a well formed zip
		newfiles[i].m_Name = tmpString;
		newfiles[i].filelen = zipFileHeader.compressedSize;
		newfiles[i].uncompressedLen = zipFileHeader.uncompressedSize;
		newfiles[i].crc32 = zipFileHeader.crc32;
		newfiles[i].filepos = zipFileHeader.relativeOffsetOfLocalHeader +
			sizeof(ZIP_LocalFileHeader) +
			zipFileHeader.fileNameLength +
			zipFileHeader.extraFieldLength;
		newfiles[i].compressionType = (IZip::eCompressionType)zipFileHeader.compressionMethod;

		int nextOffset;
		if (m_bCompatibleFormat)
		{
			nextOffset = zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength;
		}
		else
		{
			nextOffset = 0;
		}
		buf.SeekGet(CUtlBuffer::SEEK_CURRENT, nextOffset);
	}

	// Insert current data into rb tree
	for (i = 0; i < numzipfiles; i++)
	{
		CZipEntry e;
		e.m_Name = newfiles[i].m_Name;
		e.m_nCompressedSize = newfiles[i].filelen;
		e.m_ZipCRC = newfiles[i].crc32;
		e.m_nUncompressedSize = newfiles[i].uncompressedLen;
		e.m_eCompressionType = newfiles[i].compressionType;

		// Make sure length is reasonable
		if (e.m_nCompressedSize > 0)
		{
			e.m_pData = malloc(e.m_nCompressedSize);

			// Copy in data
			buf.SeekGet(CUtlBuffer::SEEK_HEAD, newfiles[i].filepos);
			buf.Get(e.m_pData, e.m_nCompressedSize);
		}
		else
		{
			e.m_pData = NULL;
		}

		// Add to tree
		m_Files.Insert(e);
	}

	// Through away directory
	delete[] newfiles;
}

//-----------------------------------------------------------------------------
// Purpose: Mount pak file from disk
//-----------------------------------------------------------------------------
HANDLE CXZipFile::OpenFromDisk(const char* pFilename)
{
	HANDLE hFile = CreateFile(pFilename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		// not found
		return NULL;
	}

	unsigned int fileLen = CWin32File::FileSeek(hFile, 0, FILE_END);
	CWin32File::FileSeek(hFile, 0, FILE_BEGIN);
	if (fileLen < sizeof(ZIP_EndOfCentralDirRecord))
	{
		// bad format
		CloseHandle(hFile);
		return NULL;
	}

	// need to get the central dir
	ZIP_EndOfCentralDirRecord rec = { 0 };
	unsigned int offset = fileLen - sizeof(ZIP_EndOfCentralDirRecord);
	// If offset is ever greater than startOffset then it means that it has
	// wrapped. This used to be a tautological >= 0 test.
	ANALYZE_SUPPRESS(6293); // warning C6293: Ill-defined for-loop: counts down from minimum
	for (unsigned int startOffset = offset; offset <= startOffset; offset--)
	{
		CWin32File::FileSeek(hFile, offset, FILE_BEGIN);

		CWin32File::FileRead(hFile, &rec, sizeof(rec));
		m_Swap.SwapFieldsToTargetEndian(&rec);

		if (rec.signature == PKID(5, 6))
		{
			// Set any xzip configuration
			if (rec.commentLength)
			{
				char commentString[128];
				int commentLength = min(rec.commentLength, sizeof(commentString));
				CWin32File::FileRead(hFile, commentString, commentLength);
				if (commentLength == sizeof(commentString))
					--commentLength;
				commentString[commentLength] = '\0';
				ParseXZipCommentString(commentString);
			}
			break;
		}
		else
		{
			// wrong record
			rec.nCentralDirectoryEntries_Total = 0;
		}
	}

	// Make sure there are some files to parse
	int numZipFiles = rec.nCentralDirectoryEntries_Total;
	if (numZipFiles <= 0)
	{
		// No files
		CloseHandle(hFile);
		return NULL;
	}

	CWin32File::FileSeek(hFile, rec.startOfCentralDirOffset, FILE_BEGIN);

	// read entire central dir into memory
	CUtlBuffer zipDirBuff(0, rec.centralDirectorySize, 0);
	zipDirBuff.ActivateByteSwapping(m_Swap.IsSwappingBytes());
	CWin32File::FileRead(hFile, zipDirBuff.Base(), rec.centralDirectorySize);
	zipDirBuff.SeekPut(CUtlBuffer::SEEK_HEAD, rec.centralDirectorySize);

	// build directory
	for (int i = 0; i < numZipFiles; i++)
	{
		ZIP_FileHeader zipFileHeader;
		zipDirBuff.GetObjects(&zipFileHeader);

		if (zipFileHeader.signature != PKID(1, 2)
			|| (zipFileHeader.compressionMethod != IZip::eCompressionType_None
				&& zipFileHeader.compressionMethod != IZip::eCompressionType_LZMA))
		{
			// bad contents
			CloseHandle(hFile);
			return NULL;
		}

		char fileName[1024];
		zipDirBuff.Get(fileName, zipFileHeader.fileNameLength);
		fileName[zipFileHeader.fileNameLength] = '\0';
		Q_strlower(fileName);

		// can determine actual filepos, assuming a well formed zip
		CZipEntry e;
		e.m_Name = fileName;
		e.m_nCompressedSize = zipFileHeader.compressedSize;
		e.m_nUncompressedSize = zipFileHeader.uncompressedSize;
		e.m_ZipCRC = zipFileHeader.crc32;
		e.m_SourceDiskOffset = zipFileHeader.relativeOffsetOfLocalHeader +
			sizeof(ZIP_LocalFileHeader) +
			zipFileHeader.fileNameLength +
			zipFileHeader.extraFieldLength;
		e.m_eCompressionType = (IZip::eCompressionType)zipFileHeader.compressionMethod;

		// Add to tree
		m_Files.Insert(e);

		int nextOffset;
		if (m_bCompatibleFormat)
		{
			nextOffset = zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength;
		}
		else
		{
			nextOffset = 0;
		}

		zipDirBuff.SeekGet(CUtlBuffer::SEEK_CURRENT, nextOffset);
	}

	return hFile;
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new lump, or overwrites existing one
// Input  : *relativename -
//			*data -
//			length -
//-----------------------------------------------------------------------------
void CXZipFile::AddBuffer(const char* relativename, void* data, int length, bool bTextMode, IZip::eCompressionType compressionType)
{
	// Lower case only
	char name[512];
	Q_strcpy(name, relativename);
	Q_strlower(name);

	int outLength = length;
	int uncompressedLength = length;
	void* outData = data;
	CUtlBuffer textTransform;
	CUtlBuffer compressionTransform;

	if (bTextMode)
	{
		int textLen = GetLengthOfBinStringAsText((const char*)outData, outLength);
		textTransform.EnsureCapacity(textLen);
		CopyTextData((char*)textTransform.Base(), (char*)outData, textLen, outLength);

		outData = (void*)textTransform.Base();
		outLength = textLen;
		uncompressedLength = textLen;
	}

	// uncompressed data final at this point (CRC is before compression)
	CRC32_t zipCRC;
	CRC32_Init(&zipCRC);
	CRC32_ProcessBuffer(&zipCRC, outData, outLength);
	CRC32_Final(&zipCRC);

#ifdef ZIP_SUPPORT_LZMA_ENCODE
	if (compressionType == IZip::eCompressionType_LZMA)
	{
		unsigned int compressedSize = 0;
		unsigned char* pCompressedOutput = LZMA_Compress((unsigned char*)outData, outLength, &compressedSize);
		if (!pCompressedOutput || compressedSize < sizeof(lzma_header_t))
		{
			Warning("ZipFile: LZMA compression failed\n");
			return;
		}

		// Fixup LZMA header for ZIP payload usage
		// The output of LZMA_Compress uses lzma_header_t, defined alongside it.
		//
		// ZIP payload format, see ZIP spec 5.8.8:
		//  LZMA Version Information 2 bytes
		//  LZMA Properties Size 2 bytes
		//  LZMA Properties Data variable, defined by "LZMA Properties Size"
		unsigned int nZIPHeader = 2 + 2 + sizeof(lzma_header_t().properties);
		unsigned int finalCompressedSize = compressedSize - sizeof(lzma_header_t) + nZIPHeader;
		compressionTransform.EnsureCapacity(finalCompressedSize);

		// LZMA version
		compressionTransform.PutUnsignedChar(LZMA_SDK_VERSION_MAJOR);
		compressionTransform.PutUnsignedChar(LZMA_SDK_VERSION_MINOR);
		// properties size
		uint16 nSwappedPropertiesSize = LittleWord(sizeof(lzma_header_t().properties));
		compressionTransform.Put(&nSwappedPropertiesSize, sizeof(nSwappedPropertiesSize));
		// properties
		compressionTransform.Put(&(((lzma_header_t*)pCompressedOutput)->properties), sizeof(lzma_header_t().properties));
		// payload
		compressionTransform.Put(pCompressedOutput + sizeof(lzma_header_t), compressedSize - sizeof(lzma_header_t));

		// Free original
		free(pCompressedOutput);
		pCompressedOutput = NULL;

		outData = (void*)compressionTransform.Base();
		outLength = finalCompressedSize;
		// (Not updating uncompressedLength)
	}
	else
#endif
		/* else from ifdef */ if (compressionType != IZip::eCompressionType_None)
		{
			Error("Calling AddBuffer with unknown compression type\n");
			return;
		}

	// See if entry is in list already
	CZipEntry e;
	e.m_Name = name;
	int index = m_Files.Find(e);

	// If already existing, throw away old data and update data and length
	if (index != m_Files.InvalidIndex())
	{
		CZipEntry* update = &m_Files[index];
		if (update->m_pData)
		{
			free(update->m_pData);
		}

		update->m_eCompressionType = compressionType;
		update->m_pData = malloc(outLength);
		memcpy(update->m_pData, outData, outLength);
		update->m_nCompressedSize = outLength;
		update->m_nUncompressedSize = uncompressedLength;
		update->m_ZipCRC = zipCRC;

		if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE)
		{
			update->m_DiskCacheOffset = CWin32File::FileTell(m_hDiskCacheWriteFile);
			CWin32File::FileWrite(m_hDiskCacheWriteFile, update->m_pData, update->m_nCompressedSize);
			free(update->m_pData);
			update->m_pData = NULL;
		}
	}
	else
	{
		// Create a new entry
		e.m_nCompressedSize = outLength;
		e.m_nUncompressedSize = uncompressedLength;
		e.m_eCompressionType = compressionType;
		e.m_ZipCRC = zipCRC;
		if (outLength > 0)
		{
			e.m_pData = malloc(outLength);
			memcpy(e.m_pData, outData, outLength);

			if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE)
			{
				e.m_DiskCacheOffset = CWin32File::FileTell(m_hDiskCacheWriteFile);
				CWin32File::FileWrite(m_hDiskCacheWriteFile, e.m_pData, e.m_nCompressedSize);
				free(e.m_pData);
				e.m_pData = NULL;
			}
		}
		else
		{
			e.m_pData = NULL;
		}

		m_Files.Insert(e);
	}
}

//-----------------------------------------------------------------------------
// Reads a file from the zip
//-----------------------------------------------------------------------------
bool CXZipFile::ReadFile(const char* pRelativeName, bool bTextMode, CUtlBuffer& buf)
{
	return ReadFile(0, pRelativeName, bTextMode, buf);
}

//-----------------------------------------------------------------------------
// Reads a file from the zip. Requires the zip file handle if this zip was loaded via OpenFromDisk
//-----------------------------------------------------------------------------
bool CXZipFile::ReadFile(HANDLE hZipFile, const char* pRelativeName, bool bTextMode, CUtlBuffer& buf)
{
	// Lower case only
	char pName[512];
	Q_strncpy(pName, pRelativeName, 512);
	Q_strlower(pName);

	// See if entry is in list already
	CZipEntry e;
	e.m_Name = pName;
	int nIndex = m_Files.Find(e);
	if (nIndex == m_Files.InvalidIndex())
	{
		// not found
		return false;
	}

	CZipEntry* pEntry = &m_Files[nIndex];

	void* pData = pEntry->m_pData;
	CUtlBuffer readBuffer;
	if (!pData && hZipFile)
	{
		readBuffer.EnsureCapacity(pEntry->m_nCompressedSize);
		CWin32File::FileSeek(hZipFile, pEntry->m_SourceDiskOffset, FILE_BEGIN);
		if (!CWin32File::FileRead(hZipFile, readBuffer.Base(), pEntry->m_nCompressedSize))
		{
			return false;
		}

		pData = readBuffer.Base();
	}

	CUtlBuffer decompressTransform;
	if (pEntry->m_eCompressionType != IZip::eCompressionType_None)
	{
		if (pEntry->m_eCompressionType == IZip::eCompressionType_LZMA)
		{
			decompressTransform.EnsureCapacity(pEntry->m_nUncompressedSize);

			CLZMAStream decompressStream;
			decompressStream.InitZIPHeader(pEntry->m_nCompressedSize, pEntry->m_nUncompressedSize);

			unsigned int nCompressedBytesRead = 0;
			unsigned int nOutputBytesWritten = 0;
			bool bSuccess = decompressStream.Read((unsigned char*)pData, pEntry->m_nCompressedSize,
				(unsigned char*)decompressTransform.Base(), decompressTransform.Size(),
				nCompressedBytesRead, nOutputBytesWritten);
			if (!bSuccess ||
				(int)nCompressedBytesRead != pEntry->m_nCompressedSize ||
				(int)nOutputBytesWritten != pEntry->m_nUncompressedSize)
			{
				Error("Zip: Failed decompressing LZMA data\n");
				return false;
			}

			pData = decompressTransform.Base();
		}
		else
		{
			Error("Unsupported compression type in Zip file: %u\n", pEntry->m_eCompressionType);
			return false;
		}
	}

	if (bTextMode)
	{
		buf.SetBufferType(true, false);
		ReadTextData((const char*)pData, pEntry->m_nUncompressedSize, buf);
	}
	else
	{
		buf.SetBufferType(false, false);
		buf.Put(pData, pEntry->m_nUncompressedSize);
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Check if a file already exists in the zip.
// Input  : *relativename -
//-----------------------------------------------------------------------------
bool CXZipFile::FileExists(const char* pRelativeName)
{
	// Lower case only
	char pName[512];
	Q_strncpy(pName, pRelativeName, 512);
	Q_strlower(pName);

	// See if entry is in list already
	CZipEntry e;
	e.m_Name = pName;
	int nIndex = m_Files.Find(e);

	// If it is, then it exists in the pack!
	return nIndex != m_Files.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new file to the zip.
//-----------------------------------------------------------------------------
void CXZipFile::AddFile(const char* relativename, const char* fullpath, IZip::eCompressionType compressionType)
{
	FILE* temp = fopen(fullpath, "rb");
	if (!temp)
		return;

	// Determine length
	fseek(temp, 0, SEEK_END);
	int size = ftell(temp);
	fseek(temp, 0, SEEK_SET);
	byte* buf = (byte*)malloc(size + 1);

	// Read data
	fread(buf, size, 1, temp);
	fclose(temp);

	// Now add as a buffer
	AddBuffer(relativename, buf, size, false, compressionType);

	free(buf);
}

//-----------------------------------------------------------------------------
// Purpose: Removes a file from the zip.
//-----------------------------------------------------------------------------
void CXZipFile::RemoveFile(const char* relativename)
{
	CZipEntry e;
	e.m_Name = relativename;
	int index = m_Files.Find(e);

	if (index != m_Files.InvalidIndex())
	{
		CZipEntry update = m_Files[index];
		m_Files.Remove(update);
	}
}

//---------------------------------------------------------------
//	Purpose: Calculates how many bytes should be added to the extra field
//  to push the start of the file data to the next aligned boundary
//  Output: Required padding size
//---------------------------------------------------------------
unsigned short CXZipFile::CalculatePadding(unsigned int filenameLen, unsigned int pos)
{
	if (m_AlignmentSize == 0)
	{
		return 0;
	}

	unsigned int headerSize = sizeof(ZIP_LocalFileHeader) + filenameLen;
	return (unsigned short)(m_AlignmentSize - ((pos + headerSize) % m_AlignmentSize));
}

//-----------------------------------------------------------------------------
// Purpose: Create the XZIP identifying comment string
// Output : Length
//-----------------------------------------------------------------------------
int CXZipFile::MakeXZipCommentString(char* pCommentString)
{
	char tempString[XZIP_COMMENT_LENGTH];

	memset(tempString, 0, sizeof(tempString));
	V_snprintf(tempString, sizeof(tempString), "XZP%c %d", m_bCompatibleFormat ? '1' : '2', m_AlignmentSize);
	if (pCommentString)
	{
		memcpy(pCommentString, tempString, sizeof(tempString));
	}

	// expected fixed length
	return XZIP_COMMENT_LENGTH;
}

//-----------------------------------------------------------------------------
// Purpose: An XZIP has its configuration in the ascii comment
//-----------------------------------------------------------------------------
void CXZipFile::ParseXZipCommentString(const char* pCommentString)
{
	if (!V_strnicmp(pCommentString, "XZP", 3))
	{
		m_bCompatibleFormat = true;
		if (pCommentString[3] == '2')
		{
			m_bCompatibleFormat = false;
		}

		// parse out the alignement configuration
		if (!m_bForceAlignment)
		{
			m_AlignmentSize = 0;
			sscanf(pCommentString + 4, "%d", &m_AlignmentSize);
			if (!IsPowerOfTwo(m_AlignmentSize))
			{
				m_AlignmentSize = 0;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the exact size of zip file, with headers and padding
// Output : int
//-----------------------------------------------------------------------------
unsigned int CXZipFile::CalculateSize(void)
{
	unsigned int size = 0;
	unsigned int dirHeaders = 0;
	for (int i = m_Files.FirstInorder(); i != m_Files.InvalidIndex(); i = m_Files.NextInorder(i))
	{
		CZipEntry* e = &m_Files[i];

		if (e->m_nCompressedSize == 0)
			continue;

		// local file header
		size += sizeof(ZIP_LocalFileHeader);
		size += strlen(e->m_Name.String());

		// every file has a directory header that duplicates the filename
		dirHeaders += sizeof(ZIP_FileHeader) + strlen(e->m_Name.String());

		// calculate padding
		if (m_AlignmentSize != 0)
		{
			// round up to next boundary
			unsigned int nextBoundary = (size + m_AlignmentSize) & ~(m_AlignmentSize - 1);

			// the directory header also duplicates the padding
			dirHeaders += nextBoundary - size;

			size = nextBoundary;
		}

		// data size
		size += e->m_nCompressedSize;
	}

	size += dirHeaders;

	// All processed zip files will have a comment string
	size += sizeof(ZIP_EndOfCentralDirRecord) + MakeXZipCommentString(NULL);

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Print a directory of files in the zip
//-----------------------------------------------------------------------------
void CXZipFile::SpewDirectory(void)
{
	for (int i = m_Files.FirstInorder(); i != m_Files.InvalidIndex(); i = m_Files.NextInorder(i))
	{
		CZipEntry* e = &m_Files[i];

		printf("%s\n", e->m_Name.String());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Iterate through directory
//-----------------------------------------------------------------------------
int CXZipFile::GetNextEntry(int id, CUtlSymbol& fileEntry, int& fileSize)
{
	if (id == -1)
	{
		id = m_Files.FirstInorder();
	}
	else
	{
		id = m_Files.NextInorder(id);
	}

	if (id == m_Files.InvalidIndex())
	{
		// list is empty
		return -1;
	}

	CZipEntry* e = &m_Files[id];
	fileEntry = e->m_Name;
	fileSize = e->m_nUncompressedSize;

	return id;
}

//-----------------------------------------------------------------------------
// Purpose: Store data out to disk
//-----------------------------------------------------------------------------
void CXZipFile::SaveToDisk(FILE* fout)
{
	CFileStream stream(fout);
	SaveDirectory(stream);
}

void CXZipFile::SaveToDisk(HANDLE hOutFile)
{
	CFileStream stream(hOutFile);
	SaveDirectory(stream);
}

//-----------------------------------------------------------------------------
// Purpose: Store data out to a CUtlBuffer
//-----------------------------------------------------------------------------
void CXZipFile::SaveToBuffer(CUtlBuffer& buf)
{
	CBufferStream stream(buf);
	SaveDirectory(stream);
}

//-----------------------------------------------------------------------------
// Purpose: Store data back out to a stream (could be CUtlBuffer or filestream)
//-----------------------------------------------------------------------------
void CXZipFile::SaveDirectory(IWriteStream& stream)
{
	void* pPaddingBuffer = NULL;
	if (m_AlignmentSize)
	{
		// get a temp buffer for all padding work
		pPaddingBuffer = malloc(m_AlignmentSize);
		memset(pPaddingBuffer, 0x00, m_AlignmentSize);
	}

	if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE)
	{
		FlushFileBuffers(m_hDiskCacheWriteFile);
	}

	// Might be writing a zip into a larger stream
	unsigned int zipOffsetInStream = stream.Tell();

	int i;
	for (i = m_Files.FirstInorder(); i != m_Files.InvalidIndex(); i = m_Files.NextInorder(i))
	{
		CZipEntry* e = &m_Files[i];
		Assert(e);

		// Fix up the offset
		e->m_ZipOffset = stream.Tell() - zipOffsetInStream;

		if (e->m_nCompressedSize > 0 && (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE))
		{
			// get the data back from the write cache
			e->m_pData = malloc(e->m_nCompressedSize);
			if (e->m_pData)
			{
				CWin32File::FileSeek(m_hDiskCacheWriteFile, e->m_DiskCacheOffset, FILE_BEGIN);
				CWin32File::FileRead(m_hDiskCacheWriteFile, e->m_pData, e->m_nCompressedSize);
			}
		}

		if (e->m_nCompressedSize > 0 && e->m_pData != NULL)
		{
			ZIP_LocalFileHeader hdr = { 0 };
			hdr.signature = PKID(3, 4);
			hdr.versionNeededToExtract = 10;  // No special features or even compression here, set to 1.0
#ifdef ZIP_SUPPORT_LZMA_ENCODE
			if (e->m_eCompressionType == IZip::eCompressionType_LZMA)
			{
				// Per ZIP spec 5.8.8
				hdr.versionNeededToExtract = 63;
			}
#endif
			hdr.flags = 0;
			hdr.compressionMethod = e->m_eCompressionType;
			hdr.lastModifiedTime = 0;
			hdr.lastModifiedDate = 0;
			hdr.crc32 = e->m_ZipCRC;

			const char* pFilename = e->m_Name.String();
			hdr.compressedSize = e->m_nCompressedSize;
			hdr.uncompressedSize = e->m_nUncompressedSize;
			hdr.fileNameLength = strlen(pFilename);
			hdr.extraFieldLength = CalculatePadding(hdr.fileNameLength, e->m_ZipOffset);
			int extraFieldLength = hdr.extraFieldLength;

			// Swap header in place
			m_Swap.SwapFieldsToTargetEndian(&hdr);
			stream.Put(&hdr, sizeof(hdr));
			stream.Put(pFilename, strlen(pFilename));
			stream.Put(pPaddingBuffer, extraFieldLength);
			stream.Put(e->m_pData, e->m_nCompressedSize);

			if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE)
			{
				free(e->m_pData);

				// temp hackery for the logic below to succeed
				e->m_pData = (void*)0xFFFFFFFF;
			}
		}
	}

	if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE)
	{
		CWin32File::FileSeek(m_hDiskCacheWriteFile, 0, FILE_END);
	}

	unsigned int centralDirStart = stream.Tell() - zipOffsetInStream;
	if (m_AlignmentSize)
	{
		// align the central directory starting position
		unsigned int newDirStart = AlignValue(centralDirStart, m_AlignmentSize);
		int padLength = newDirStart - centralDirStart;
		if (padLength)
		{
			stream.Put(pPaddingBuffer, padLength);
			centralDirStart = newDirStart;
		}
	}

	int realNumFiles = 0;
	for (i = m_Files.FirstInorder(); i != m_Files.InvalidIndex(); i = m_Files.NextInorder(i))
	{
		CZipEntry* e = &m_Files[i];
		Assert(e);

		if (e->m_nCompressedSize > 0 && e->m_pData != NULL)
		{
			ZIP_FileHeader hdr = { 0 };
			hdr.signature = PKID(1, 2);
			hdr.versionMadeBy = 20;				// This is the version that the winzip that I have writes.
			hdr.versionNeededToExtract = 10;  // No special features or even compression here, set to 1.0
#ifdef ZIP_SUPPORT_LZMA_ENCODE
			if (e->m_eCompressionType == IZip::eCompressionType_LZMA)
			{
				// Per ZIP spec 5.8.8
				hdr.versionNeededToExtract = 63;
			}
#endif
			hdr.flags = 0;
			hdr.compressionMethod = e->m_eCompressionType;
			hdr.lastModifiedTime = 0;
			hdr.lastModifiedDate = 0;
			hdr.crc32 = e->m_ZipCRC;

			hdr.compressedSize = e->m_nCompressedSize;
			hdr.uncompressedSize = e->m_nUncompressedSize;
			hdr.fileNameLength = strlen(e->m_Name.String());
			hdr.extraFieldLength = CalculatePadding(hdr.fileNameLength, e->m_ZipOffset);
			hdr.fileCommentLength = 0;
			hdr.diskNumberStart = 0;
			hdr.internalFileAttribs = 0;
			hdr.externalFileAttribs = 0; // This is usually something, but zero is OK as if the input came from stdin
			hdr.relativeOffsetOfLocalHeader = e->m_ZipOffset;
			int extraFieldLength = hdr.extraFieldLength;

			// Swap the header in place
			m_Swap.SwapFieldsToTargetEndian(&hdr);
			stream.Put(&hdr, sizeof(hdr));
			stream.Put(e->m_Name.String(), strlen(e->m_Name.String()));
			if (m_bCompatibleFormat)
			{
				stream.Put(pPaddingBuffer, extraFieldLength);
			}

			realNumFiles++;

			if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE)
			{
				// clear out temp hackery
				e->m_pData = NULL;
			}
		}
	}

	unsigned int centralDirEnd = stream.Tell() - zipOffsetInStream;
	if (m_AlignmentSize)
	{
		// align the central directory starting position
		unsigned int newDirEnd = AlignValue(centralDirEnd, m_AlignmentSize);
		int padLength = newDirEnd - centralDirEnd;
		if (padLength)
		{
			stream.Put(pPaddingBuffer, padLength);
			centralDirEnd = newDirEnd;
		}
	}

	ZIP_EndOfCentralDirRecord rec = { 0 };
	rec.signature = PKID(5, 6);
	rec.numberOfThisDisk = 0;
	rec.numberOfTheDiskWithStartOfCentralDirectory = 0;
	rec.nCentralDirectoryEntries_ThisDisk = realNumFiles;
	rec.nCentralDirectoryEntries_Total = realNumFiles;
	rec.centralDirectorySize = centralDirEnd - centralDirStart;
	rec.startOfCentralDirOffset = centralDirStart;

	char commentString[128];
	int commentLength = MakeXZipCommentString(commentString);
	rec.commentLength = commentLength;

	// Swap the header in place
	m_Swap.SwapFieldsToTargetEndian(&rec);
	stream.Put(&rec, sizeof(rec));
	stream.Put(commentString, commentLength);

	if (pPaddingBuffer)
	{
		free(pPaddingBuffer);
	}
}