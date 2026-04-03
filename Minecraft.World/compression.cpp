#include "stdafx.h"
#include "compression.h"
#if defined __ORBIS__ || defined __PS3__ || defined _DURANGO || defined _WIN64
#include "../Minecraft.Client/Common/zlib/zlib.h"
#endif

#if defined __PSVITA__
#include "../Minecraft.Client/PSVita/PSVitaExtras/zlib.h"
#elif defined __PS3__
#include "../Minecraft.Client/PS3/PS3Extras/EdgeZLib.h"
#endif //__PS3__


DWORD Compression::tlsIdx = 0;
Compression::ThreadStorage *Compression::tlsDefault = NULL;

Compression::ThreadStorage::ThreadStorage()
{
	compression = new Compression();
}

Compression::ThreadStorage::~ThreadStorage()
{
	delete compression;
}

void Compression::CreateNewThreadStorage()
{
	ThreadStorage *tls = new ThreadStorage();
	if(tlsDefault == NULL )
	{
		tlsIdx = TlsAlloc();
		tlsDefault = tls;
	}
	TlsSetValue(tlsIdx, tls);
}

void Compression::UseDefaultThreadStorage()
{
	TlsSetValue(tlsIdx, tlsDefault);
}

void Compression::ReleaseThreadStorage()
{
	ThreadStorage *tls = (ThreadStorage *)TlsGetValue(tlsIdx);
	if( tls == tlsDefault ) return;

	delete tls;
}

Compression *Compression::getCompression()
{
	ThreadStorage *tls = (ThreadStorage *)TlsGetValue(tlsIdx);
	return tls->compression;
}

HRESULT Compression::CompressLZXRLE(void *pDestination, unsigned int *pDestSize, void *pSource, unsigned int SrcSize)
{
	EnterCriticalSection(&rleCompressLock);

	unsigned char *pucIn = (unsigned char *)pSource;
	unsigned char *pucEnd = pucIn + SrcSize;
	unsigned char *pucOut = (unsigned char *)rleCompressBuf;
	unsigned char *pucOutEnd = pucOut + sizeof(rleCompressBuf);

	// Compress with RLE first:
	// 0 - 254 - encodes a single byte
	// 255 followed by 0, 1, 2 - encodes a 1, 2, or 3 255s
	// 255 followed by 3-255, followed by a byte - encodes a run of n + 1 bytes
	PIXBeginNamedEvent(0,"RLE compression");
	do
	{
		unsigned char thisOne = *pucIn++;

		unsigned int count = 1;
		while( ( pucIn != pucEnd ) && ( *pucIn == thisOne ) && ( count < 256 ) )
		{
			pucIn++;
			count++;
		}

		if( count <= 3 )
		{
			if( thisOne == 255 )
			{
				if (pucOut + 2 > pucOutEnd) { LeaveCriticalSection(&rleCompressLock); return E_FAIL; }
				*pucOut++ = 255;
				*pucOut++ = count - 1;
			}
			else
			{
				if (pucOut + count > pucOutEnd) { LeaveCriticalSection(&rleCompressLock); return E_FAIL; }
				for( unsigned int i = 0; i < count ; i++ )
				{
					*pucOut++ = thisOne;
				}
			}
		}
		else
		{
			if (pucOut + 3 > pucOutEnd) { LeaveCriticalSection(&rleCompressLock); return E_FAIL; }
			*pucOut++ = 255;
			*pucOut++ = count - 1;
			*pucOut++ = thisOne;
		}
	} while (pucIn != pucEnd);
	unsigned int rleSize = (unsigned int)(pucOut - rleCompressBuf);
	PIXEndNamedEvent();

	PIXBeginNamedEvent(0,"Secondary compression");
	Compress(pDestination, pDestSize, rleCompressBuf, rleSize);
	PIXEndNamedEvent();
	LeaveCriticalSection(&rleCompressLock);
//	printf("Compressed from %d to %d to %d\n",SrcSize,rleSize,*pDestSize);

	return S_OK;
}

HRESULT Compression::CompressRLE(void *pDestination, unsigned int *pDestSize, void *pSource, unsigned int SrcSize)
{
	EnterCriticalSection(&rleCompressLock);

	unsigned char *pucIn = (unsigned char *)pSource;
	unsigned char *pucEnd = pucIn + SrcSize;
	unsigned char *pucOut = (unsigned char *)rleCompressBuf;
	unsigned char *pucOutEnd = pucOut + sizeof(rleCompressBuf);

	// Compress with RLE first:
	// 0 - 254 - encodes a single byte
	// 255 followed by 0, 1, 2 - encodes a 1, 2, or 3 255s
	// 255 followed by 3-255, followed by a byte - encodes a run of n + 1 bytes
	PIXBeginNamedEvent(0,"RLE compression");
	do
	{
		unsigned char thisOne = *pucIn++;

		unsigned int count = 1;
		while( ( pucIn != pucEnd ) && ( *pucIn == thisOne ) && ( count < 256 ) )
		{
			pucIn++;
			count++;
		}

		if( count <= 3 )
		{
			if( thisOne == 255 )
			{
				if (pucOut + 2 > pucOutEnd) { LeaveCriticalSection(&rleCompressLock); return E_FAIL; }
				*pucOut++ = 255;
				*pucOut++ = count - 1;
			}
			else
			{
				if (pucOut + count > pucOutEnd) { LeaveCriticalSection(&rleCompressLock); return E_FAIL; }
				for( unsigned int i = 0; i < count ; i++ )
				{
					*pucOut++ = thisOne;
				}
			}
		}
		else
		{
			if (pucOut + 3 > pucOutEnd) { LeaveCriticalSection(&rleCompressLock); return E_FAIL; }
			*pucOut++ = 255;
			*pucOut++ = count - 1;
			*pucOut++ = thisOne;
		}
	} while (pucIn != pucEnd);
	unsigned int rleSize = (unsigned int)(pucOut - rleCompressBuf);
	PIXEndNamedEvent();
	LeaveCriticalSection(&rleCompressLock);

	// Return
	if (rleSize <= *pDestSize)
	{
		*pDestSize = rleSize;
		memcpy(pDestination, rleCompressBuf, *pDestSize);
	}
	else
	{
		*pDestSize = 0;
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Compression::DecompressLZXRLE(void *pDestination, unsigned int *pDestSize, void *pSource, unsigned int SrcSize)
{
	EnterCriticalSection(&rleDecompressLock);
	// 4J Stu - Fix for #13676 - Crash: Crash while attempting to load a world after updating TU
	// Some saves can have chunks that decompress into very large sizes, so I have doubled the size of this buffer
	// Ideally we should be able to dynamically allocate the buffer if it's going to be too big, as most chunks
	// only use 5% of this buffer

	// 4J Stu - Changed this again to dynamically allocate a buffer if it's going to be too big
	unsigned char *pucIn = NULL;

	//const unsigned int staticRleSize = 1024*200;
	//static unsigned char rleBuf[staticRleSize];
	unsigned int rleSize = staticRleSize;
	unsigned char *dynamicRleBuf = NULL;

	if(*pDestSize > rleSize)
	{
		rleSize = *pDestSize;
		dynamicRleBuf = new unsigned char[rleSize];
		Decompress(dynamicRleBuf, &rleSize, pSource, SrcSize);
		pucIn = (unsigned char *)dynamicRleBuf;
	}
	else
	{
		Decompress(rleDecompressBuf, &rleSize, pSource, SrcSize);
		pucIn = (unsigned char *)rleDecompressBuf;
	}

	unsigned char *pucEnd = pucIn + rleSize;
	unsigned char *pucOut = (unsigned char *)pDestination;
	unsigned char *pucOutEnd = pucOut + *pDestSize;

	HRESULT hr = S_OK;
	while( pucIn < pucEnd )
	{
		unsigned char thisOne = *pucIn++;
		if( thisOne == 255 )
		{
			if (pucIn >= pucEnd) { hr = E_FAIL; break; }
			unsigned int count = *pucIn++;
			if( count < 3 )
			{
				count++;
				if (pucOut + count > pucOutEnd) { hr = E_FAIL; break; }
				for( unsigned int i = 0; i < count; i++ )
				{
					*pucOut++ = 255;
				}
			}
			else
			{
				count++;
				if (pucIn >= pucEnd) { hr = E_FAIL; break; }
				unsigned char data = *pucIn++;
				if (pucOut + count > pucOutEnd) { hr = E_FAIL; break; }
				for( unsigned int i = 0; i < count; i++ )
				{
					*pucOut++ = data;
				}
			}
		}
		else
		{
			if (pucOut >= pucOutEnd) { hr = E_FAIL; break; }
			*pucOut++ = thisOne;
		}
	}
	*pDestSize = (unsigned int)(pucOut - (unsigned char *)pDestination);

	if(dynamicRleBuf != NULL) delete [] dynamicRleBuf;

	LeaveCriticalSection(&rleDecompressLock);
	return hr;
}

HRESULT Compression::DecompressRLE(void *pDestination, unsigned int *pDestSize, void *pSource, unsigned int SrcSize)
{
	EnterCriticalSection(&rleDecompressLock);

	unsigned char *pucIn  = (unsigned char *)pSource;
	unsigned char *pucEnd = pucIn + SrcSize;
	unsigned char *pucOut = (unsigned char *)pDestination;
	unsigned char *pucOutEnd = pucOut + *pDestSize;

	HRESULT hr = S_OK;
	while( pucIn < pucEnd )
	{
		unsigned char thisOne = *pucIn++;
		if( thisOne == 255 )
		{
			if (pucIn >= pucEnd) { hr = E_FAIL; break; }
			unsigned int count = *pucIn++;
			if( count < 3 )
			{
				count++;
				if (pucOut + count > pucOutEnd) { hr = E_FAIL; break; }
				for( unsigned int i = 0; i < count; i++ )
				{
					*pucOut++ = 255;
				}
			}
			else
			{
				count++;
				if (pucIn >= pucEnd) { hr = E_FAIL; break; }
				unsigned char data = *pucIn++;
				if (pucOut + count > pucOutEnd) { hr = E_FAIL; break; }
				for( unsigned int i = 0; i < count; i++ )
				{
					*pucOut++ = data;
				}
			}
		}
		else
		{
			if (pucOut >= pucOutEnd) { hr = E_FAIL; break; }
			*pucOut++ = thisOne;
		}
	}
	*pDestSize = (unsigned int)(pucOut - (unsigned char *)pDestination);

	LeaveCriticalSection(&rleDecompressLock);
	return hr;
}


HRESULT Compression::Compress(void *pDestination, unsigned int *pDestSize, void *pSource, unsigned int SrcSize)
{
	// Using zlib for x64 compression - 360 is using native 360 compression and PS3 a stubbed non-compressing version of this
#if defined __ORBIS__ || defined _DURANGO || defined _WIN64 || defined __PSVITA__
	SIZE_T destSize = (SIZE_T)(*pDestSize);
	int res = ::compress((Bytef *)pDestination, (uLongf *)&destSize, (Bytef *)pSource, SrcSize);
	*pDestSize = (unsigned int)destSize;
	return ( ( res == Z_OK ) ? S_OK : -1 );
#elif defined __PS3__
	uint32_t destSize = (uint32_t)(*pDestSize);
	bool res = EdgeZLib::Compress(pDestination, &destSize, pSource, SrcSize);
	*pDestSize = (unsigned int)destSize;
	return ( ( res ) ? S_OK : -1 );
#else
	SIZE_T destSize = (SIZE_T)(*pDestSize);
	HRESULT res = XMemCompress(compressionContext, pDestination, &destSize, pSource, SrcSize);
	*pDestSize = (unsigned int)destSize;
	return res;
#endif
}

HRESULT Compression::Decompress(void *pDestination, unsigned int *pDestSize, void *pSource, unsigned int SrcSize)
{

	if(m_decompressType != m_localDecompressType)	// check if we're decompressing data from a different platform
	{
		// only used for loading a save from a different platform (Sony cloud storage cross play)
		return DecompressWithType(pDestination, pDestSize, pSource, SrcSize);
	}

	// Using zlib for x64 compression - 360 is using native 360 compression and PS3 a stubbed non-compressing version of this
#if defined __ORBIS__ || defined _DURANGO || defined _WIN64 || defined __PSVITA__
	SIZE_T destSize = (SIZE_T)(*pDestSize);
	int res = ::uncompress((Bytef *)pDestination, (uLongf *)&destSize, (Bytef *)pSource, SrcSize);
	*pDestSize = (unsigned int)destSize;
	return ( ( res == Z_OK ) ? S_OK : -1 );
#elif defined __PS3__
	uint32_t destSize = (uint32_t)(*pDestSize);
	bool res = EdgeZLib::Decompress(pDestination, &destSize, pSource, SrcSize);
	*pDestSize = (unsigned int)destSize;
	return ( ( res ) ? S_OK : -1 );
#else
	SIZE_T destSize = (SIZE_T)(*pDestSize);
	HRESULT res = XMemDecompress(decompressionContext, pDestination, (SIZE_T *)&destSize, pSource, SrcSize);
	*pDestSize = (unsigned int)destSize;
	return res;
#endif
}

// MGH -  same as VirtualDecompress in PSVitaStubs, but for use on other platforms (so no virtual mem stuff)
#ifndef _XBOX
VOID Compression::VitaVirtualDecompress(void *pDestination, unsigned int *pDestSize, void *pSource, unsigned int SrcSize)
{
	uint8_t *pSrc = (uint8_t *)pSource;
	unsigned int destCapacity = *pDestSize;
	unsigned int Offset = 0;
	unsigned int Index = 0;
	uint8_t* Data = (uint8_t*)pDestination;
	while( Index < SrcSize )
	{
		if( pSrc[Index] )
		{
			if (Offset >= destCapacity) break;
			Data[Offset] = pSrc[Index];
			Offset += 1;
		}
		else
		{
			Index += 1;
			if (Index >= SrcSize) break;
			int Count = pSrc[Index];
			for( int i = 0; i < Count; i += 1 )
			{
				if (Offset >= destCapacity) break;
				Data[Offset] = 0;
				Offset += 1;
			}
		}
		Index += 1;
	}
	*pDestSize = Offset;
}
#endif


HRESULT Compression::DecompressWithType(void *pDestination, unsigned int *pDestSize, void *pSource, unsigned int SrcSize)
{
	switch(m_decompressType)
	{
	case eCompressionType_RLE:
		return DecompressRLE(pDestination, pDestSize, pSource, SrcSize);
	case eCompressionType_None:
		memcpy(pDestination,pSource,SrcSize);
		*pDestSize = SrcSize;
		return S_OK;
	case eCompressionType_LZXRLE:
		{
#if (defined _XBOX || defined _DURANGO || defined _WIN64)
			SIZE_T destSize = (SIZE_T)(*pDestSize);
			HRESULT res = XMemDecompress(decompressionContext, pDestination, (SIZE_T *)&destSize, pSource, SrcSize);
			*pDestSize = (unsigned int)destSize;
			return res;
#else
			assert(0);
#endif
		}
		break;
	case eCompressionType_ZLIBRLE:
#if (defined __ORBIS__ || defined __PS3__ || defined _DURANGO || defined _WIN64)
		if (pDestination != NULL)
			return ::uncompress((PBYTE)pDestination, (unsigned long *) pDestSize, (PBYTE) pSource, SrcSize); // Decompress
		else break; // Cannot decompress when destination is NULL
#else
		assert(0);
		break;
#endif
	case eCompressionType_PS3ZLIB:
#if (defined __ORBIS__ || defined __PSVITA__ || defined _DURANGO || defined _WIN64)
		// Note that we're missing the normal zlib header and footer so we'll use inflate to 
		// decompress the payload and skip all the CRC checking, etc
		if (pDestination != NULL)
		{
			// Read big-endian srcize from array
			PBYTE pbDestSize = (PBYTE) pDestSize;
			PBYTE pbSource = (PBYTE) pSource;
			for (int i = 3; i >= 0; i--) {
				pbDestSize[3-i] = pbSource[i];
			}

			byteArray uncompr = byteArray(*pDestSize);

			// Build decompression stream
			z_stream strm;
			strm.zalloc = Z_NULL;
			strm.zfree = Z_NULL;
			strm.opaque = Z_NULL;
			strm.next_out = uncompr.data;
			strm.avail_out = uncompr.length;
			// Skip those first 4 bytes
			strm.next_in = (PBYTE) pSource + 4;
			strm.avail_in = SrcSize - 4;

			int hr = inflateInit2(&strm, -15);

			// Run inflate() on input until end of stream
			do {
				hr = inflate(&strm, Z_NO_FLUSH);

				// Check
				switch (hr) {
				case Z_NEED_DICT:
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
				case Z_STREAM_ERROR:
					(void)inflateEnd(&strm);
					assert(false);
					return E_FAIL;
				}
			} while (hr != Z_STREAM_END);

			inflateEnd(&strm);		// MGH - added, to clean up zlib, was causing a leak on Vita when dowloading a PS3 save

			// Copy the uncompressed data to the destination
			memcpy(pDestination, uncompr.data, uncompr.length);
			*pDestSize = uncompr.length;

			// Delete uncompressed data
			delete[] uncompr.data;
			return S_OK;
		}
		else break; // Cannot decompress when destination is NULL
#else 
		assert(0);
#endif
	}

	assert(false); 
	return -1;
}



Compression::Compression()
{
	// Using zlib for x64 compression - 360 is using native 360 compression and PS3 a stubbed non-compressing version of this
#if !(defined __ORBIS__ || defined __PS3__)
	// The default parameters for compression context allocated about 6.5MB, reducing the partition size here from the default 512KB to 128KB
	// brings this down to about 3MB
	XMEMCODEC_PARAMETERS_LZX params;
	params.Flags = 0;
	params.WindowSize = 128 * 1024;
	params.CompressionPartitionSize = 128 * 1024;

	XMemCreateCompressionContext(XMEMCODEC_LZX,&params,0,&compressionContext);
	XMemCreateDecompressionContext(XMEMCODEC_LZX,&params,0,&decompressionContext);
#endif

#if defined _XBOX
	m_localDecompressType = eCompressionType_LZXRLE;
#elif defined __PS3__
	m_localDecompressType = eCompressionType_PS3ZLIB;
#else
	m_localDecompressType = eCompressionType_ZLIBRLE;
#endif
	m_decompressType = m_localDecompressType;

	InitializeCriticalSection(&rleCompressLock);
	InitializeCriticalSection(&rleDecompressLock);
}

Compression::~Compression()
{
#if !(defined __ORBIS__ || defined __PS3__ || defined __PSVITA__)

	XMemDestroyCompressionContext(compressionContext);
	XMemDestroyDecompressionContext(decompressionContext);
#endif
	DeleteCriticalSection(&rleCompressLock);
	DeleteCriticalSection(&rleDecompressLock);
}



void Compression::SetDecompressionType(ESavePlatform platform)
{
	switch(platform)
	{
	case SAVE_FILE_PLATFORM_X360:
		Compression::getCompression()->SetDecompressionType(Compression::eCompressionType_LZXRLE);
		break;
	case SAVE_FILE_PLATFORM_PS3:
		Compression::getCompression()->SetDecompressionType(Compression::eCompressionType_PS3ZLIB);
		break;
	case SAVE_FILE_PLATFORM_XBONE:
	case SAVE_FILE_PLATFORM_PS4:
	case SAVE_FILE_PLATFORM_PSVITA:
	case SAVE_FILE_PLATFORM_WIN64:
		Compression::getCompression()->SetDecompressionType(Compression::eCompressionType_ZLIBRLE);
		break;
	default:
		assert(0);
		break;
	}
}

/*Compression gCompression;*/


