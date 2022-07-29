/*****************************************************************//**
 * \file   xzip_file.h
 * \brief  XZip format support. Based on the zip code provided
 *			by the public SDK.
 *
 * \author Tom <intrinsic.dev@outlook.com>
 * \date   July 2022
 *********************************************************************/
#ifndef _XZIP_FILE_H
#define _XZIP_FILE_H

#pragma once

#include "source_sdk.h"

#include "byteswap.h"
#include "checksum_crc.h"
#include "lzmaDecoder.h"
#include "utlbuffer.h"
#include "utllinkedlist.h"

#include "zip_utils.h"
#include "zip_uncompressed.h"

 /**
  * Max files allowed in a zip file (per SDK).
  */
#define MAX_FILES_IN_ZIP 32768

  /**
   * XZip Package.
   */
class CXZipFile
{
public:
	/**
	 * Default constructor.
	 *
	 * \param pDiskCacheWritePath
	 * \param bSortByName
	 */
	CXZipFile(const char* pDiskCacheWritePath, bool bSortByName);
	/**
	 * Default deconstructor.
	 *
	 */
	~CXZipFile(void);

public:
	/**
	 * Adds file to zip as a file with given name.
	 *
	 * \param relativename		Relative name (path + name) to use in the zip package
	 * \param fullpath			Full path to the file on the file system
	 * \param compressionType	Compression method to use (if any)
	 */
	void			AddFile(const char* relativename, const char* fullpath, IZip::eCompressionType compressionType);
	/**
	 * Add buffer to zip as a file with given name.
	 *
	 * \param relativename		Relative name (path + name) to use in the zip package
	 * \param data				Buffer containing file contents
	 * \param length			Length of buffer
	 * \param bTextMode			True to read as text, false to read raw
	 * \param compressionType	Compression method to use (if any)
	 */
	void			AddBuffer(const char* relativename, void* data, int length, bool bTextMode, IZip::eCompressionType compressionType);
	/**
	 * Removes all file entries from the zip.
	 *
	 */
	void			Clear(void);
	/**
	 * Removes a given file from the zip.
	 *
	 * \param relativename	Relative name (path + name) to use in the zip package
	 */
	void			RemoveFile(const char* relativename);

	bool			FileExists(const char* relativename);

	bool			ReadFile(const char* relativename, bool bTextMode, CUtlBuffer& buf);
	bool			ReadFile(HANDLE hZipFile, const char* relativename, bool bTextMode, CUtlBuffer& buf);

	void			OpenFromBuffer(void* buffer, int bufferlength);
	HANDLE			OpenFromDisk(const char* pFilename);

	void			SpewDirectory(void);

	int				GetNextFilename(int id, char* pBuffer, int bufferSize, int& fileSize);

	void			SaveToBuffer(CUtlBuffer& buffer);
	void			SaveToDisk(FILE* fout);
	void			SaveToDisk(HANDLE hOutFile);

	unsigned int	CalculateSize(void);
	void			ForceAlignment(bool aligned, bool bCompatibleFormat, unsigned int alignmentSize);
	unsigned int	GetAlignment();
	void			SetBigEndian(bool bigEndian);
	void			ActivateByteSwapping(bool bActivate);

private:
	typedef struct
	{
		CUtlSymbol             m_Name;
		unsigned int           filepos;
		int                    filelen;
		int                    uncompressedLen;
		CRC32_t                crc32;
		IZip::eCompressionType compressionType;
	} TmpFileInfo_t;

	CByteswap		m_Swap;
	unsigned int	m_AlignmentSize;
	bool			m_bForceAlignment;
	bool			m_bCompatibleFormat;

	unsigned short	CalculatePadding(unsigned int filenameLen, unsigned int pos);
	void			SaveDirectory(IWriteStream& stream);
	int				MakeXZipCommentString(char* pComment);
	void			ParseXZipCommentString(const char* pComment);

	/**
	 * Internal entry for faster searching, etc.
	 */
	class CZipEntry
	{
	public:
		CZipEntry(void);
		~CZipEntry(void);

		CZipEntry(const CZipEntry& src);

		static bool ZipFileLessFunc(CZipEntry const& src1, CZipEntry const& src2);
		static bool ZipFileLessFunc_CaselessSort(CZipEntry const& src1, CZipEntry const& src2);

		// Name of entry
		CUtlSymbol		m_Name;

		// Lenth of data element
		int				m_nCompressedSize;

		// Original, uncompressed size
		int				m_nUncompressedSize;

		// Raw data, could be null and data may be in disk write cache
		void* m_pData;

		// Offset in Zip ( set and valid during final write )
		unsigned int	m_ZipOffset;
		// CRC of blob
		CRC32_t			m_ZipCRC;

		// Location of data in disk cache
		unsigned int	m_DiskCacheOffset;
		unsigned int	m_SourceDiskOffset;

		IZip::eCompressionType m_eCompressionType;
	};

	CUtlRBTree< CZipEntry, int > m_Files;

	bool				m_bUseDiskCacheForWrites;
	HANDLE				m_hDiskCacheWriteFile;
	CUtlString			m_DiskCacheName;
	CUtlString			m_DiskCacheWritePath;
};

/**
 * Holds the current zips.
 *
 * todo(intrinsic): This needs to be removed..
 */
static CUtlLinkedList< CXZipFile* > g_ZipUtils;

#endif // _XZIP_FILE_H
