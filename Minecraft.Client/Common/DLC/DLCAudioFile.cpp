#include "stdafx.h"
#include "DLCManager.h"
#include "DLCAudioFile.h"
#if defined _XBOX || defined _WINDOWS64
#include "../../Xbox/XML/ATGXmlParser.h"
#include "../../Xbox/XML/xmlFilesCallback.h"
#endif

DLCAudioFile::DLCAudioFile(const wstring &path) : DLCFile(DLCManager::e_DLCType_Audio,path)
{	
	m_pbData = nullptr;
	m_dwBytes = 0;
}

void DLCAudioFile::addData(PBYTE pbData, DWORD dwBytes)
{
	m_pbData = pbData;
	m_dwBytes = dwBytes;

	processDLCDataFile(pbData,dwBytes);
}

PBYTE DLCAudioFile::getData(DWORD &dwBytes)
{
	dwBytes = m_dwBytes;
	return m_pbData;
}

const WCHAR *DLCAudioFile::wchTypeNamesA[]=
{
	L"CUENAME",
	L"CREDIT",
};

DLCAudioFile::EAudioParameterType DLCAudioFile::getParameterType(const wstring &paramName)
{
	EAudioParameterType type = e_AudioParamType_Invalid;

	for(DWORD i = 0; i < e_AudioParamType_Max; ++i)
	{
		if(paramName.compare(wchTypeNamesA[i]) == 0)
		{
			type = static_cast<EAudioParameterType>(i);
			break;
		}
	}

	return type;
}

void DLCAudioFile::addParameter(EAudioType type, EAudioParameterType ptype, const wstring &value)
{
	switch(ptype)
	{
		case e_AudioParamType_Cuename:
			if (type < 0 || type >= e_AudioType_Max)
				return;
			m_parameters[type].push_back(value);
			break;

		case e_AudioParamType_Credit: // If this parameter exists, then mark this as free
			//add it to the DLC credits list

			// we'll need to justify this text since we don't have a lot of room for lines of credits
			{
				// don't look for duplicate in the music credits

				//if(app.AlreadySeenCreditText(value)) break;

				int maximumChars = 55;

				bool bIsSDMode=!RenderManager.IsHiDef() && !RenderManager.IsWidescreen();

				if(bIsSDMode)
				{
					maximumChars = 45;
				}

				switch(XGetLanguage())
				{
				case XC_LANGUAGE_JAPANESE:
				case XC_LANGUAGE_TCHINESE:
				case XC_LANGUAGE_KOREAN:
					maximumChars = 35;
					break;
				}
				wstring creditValue = value;
				while (creditValue.length() > maximumChars)
				{
					unsigned int i = 1;
					while (i < creditValue.length() && (i + 1) <= maximumChars)
					{
						i++;
					}
					size_t iLast=creditValue.find_last_of(L" ", i);
					switch(XGetLanguage())
					{
					case XC_LANGUAGE_JAPANESE:
					case XC_LANGUAGE_TCHINESE:
					case XC_LANGUAGE_KOREAN:
						iLast = maximumChars;
						break;
					default:
						iLast=creditValue.find_last_of(L" ", i);
						break;
					}

					// if a space was found, include the space on this line
					if(iLast!=i)
					{
						iLast++;
					}

					app.AddCreditText((creditValue.substr(0, iLast)).c_str());
					creditValue = creditValue.substr(iLast);
				}
				app.AddCreditText(creditValue.c_str());

			}
			break;
		default:
			break;
	}
}

bool DLCAudioFile::processDLCDataFile(PBYTE pbData, DWORD dwLength)
{
	unordered_map<int, EAudioParameterType> parameterMapping;
	unsigned int uiCurrentByte=0;

	// File format defined in the AudioPacker
	// File format: Version 1

	unsigned int uiVersion=*(unsigned int *)pbData;
	uiCurrentByte+=sizeof(int);

	if(uiVersion < CURRENT_AUDIO_VERSION_NUM)
	{
		// pbData is owned by the parent DLCPack buffer — do not delete here (would corrupt heap).
		app.DebugPrintf("DLC version of %d is too old to be read\n", uiVersion);
		return false;
	}
	
	unsigned int uiParameterTypeCount=*(unsigned int *)&pbData[uiCurrentByte];
	uiCurrentByte+=sizeof(int);
	C4JStorage::DLC_FILE_PARAM *pParams = nullptr;

	for(unsigned int i=0;i<uiParameterTypeCount;i++)
	{
		if (uiCurrentByte + 2u * sizeof(DWORD) > dwLength)
			return false;
		pParams = (C4JStorage::DLC_FILE_PARAM *)&pbData[uiCurrentByte];
		const DWORD nameWch = pParams->dwWchCount;
		const size_t mapStride = sizeof(C4JStorage::DLC_FILE_PARAM) + (size_t)nameWch * sizeof(WCHAR);
		if (mapStride < sizeof(C4JStorage::DLC_FILE_PARAM) || uiCurrentByte + mapStride > dwLength)
			return false;
		// wchData is length-prefixed; not guaranteed null-terminated (do not use wchar_t* ctor alone).
		wstring parameterName(pParams->wchData, nameWch);
		EAudioParameterType type = getParameterType(parameterName);
		if( type != e_AudioParamType_Invalid )
		{
			parameterMapping[pParams->dwType] = type;
		}
		uiCurrentByte += mapStride;
	}
	unsigned int uiFileCount=*(unsigned int *)&pbData[uiCurrentByte];
	uiCurrentByte+=sizeof(int);
	if (uiCurrentByte > dwLength)
		return false;
	C4JStorage::DLC_FILE_DETAILS *pFile = nullptr;

	DWORD dwTemp = uiCurrentByte;
	for (unsigned int i = 0; i < uiFileCount; i++)
	{
		if (dwTemp + sizeof(C4JStorage::DLC_FILE_DETAILS) > dwLength)
			return false;
		pFile = (C4JStorage::DLC_FILE_DETAILS *)&pbData[dwTemp];
		const DWORD pathWch = pFile->dwWchCount;
		const size_t detailStride = sizeof(C4JStorage::DLC_FILE_DETAILS) + (size_t)pathWch * sizeof(WCHAR);
		if (detailStride < sizeof(C4JStorage::DLC_FILE_DETAILS) || dwTemp + detailStride > dwLength)
			return false;
		dwTemp += static_cast<DWORD>(detailStride);
	}
	// First byte after all DLC_FILE_DETAILS headers (matches original scan loop end state).
	PBYTE pbTemp = pbData + dwTemp;
	pFile = (C4JStorage::DLC_FILE_DETAILS *)&pbData[uiCurrentByte];

	for(unsigned int i=0;i<uiFileCount;i++)
	{
		if (uiCurrentByte + sizeof(C4JStorage::DLC_FILE_DETAILS) > dwLength)
			return false;
		pFile = (C4JStorage::DLC_FILE_DETAILS *)&pbData[uiCurrentByte];
		const DWORD pathWch = pFile->dwWchCount;
		const size_t detailStride = sizeof(C4JStorage::DLC_FILE_DETAILS) + (size_t)pathWch * sizeof(WCHAR);
		if (detailStride < sizeof(C4JStorage::DLC_FILE_DETAILS) || uiCurrentByte + detailStride > dwLength)
			return false;

		EAudioType type = static_cast<EAudioType>(pFile->dwType);
		// Params
		if (pbTemp + sizeof(unsigned int) > pbData + dwLength)
			return false;
		unsigned int uiParameterCount=*(unsigned int *)pbTemp;
		pbTemp+=sizeof(int);
		for(unsigned int j=0;j<uiParameterCount;j++)
		{
			if (pbTemp + 2u * sizeof(DWORD) > pbData + dwLength)
				return false;
			pParams = (C4JStorage::DLC_FILE_PARAM *)pbTemp;
			const DWORD valWch = pParams->dwWchCount;
			const size_t paramStride = sizeof(C4JStorage::DLC_FILE_PARAM) + (size_t)valWch * sizeof(WCHAR);
			if (paramStride < sizeof(C4JStorage::DLC_FILE_PARAM) || pbTemp + paramStride > pbData + dwLength)
				return false;

			auto it = parameterMapping.find(pParams->dwType);

			if(it != parameterMapping.end() )
			{
				wstring paramValue(pParams->wchData, valWch);
				addParameter(type, it->second, paramValue);
			}
			pbTemp += paramStride;
		}
		// Move the pointer to the start of the next files data;
		if (pbTemp + pFile->uiFileSize < pbTemp || pbTemp + pFile->uiFileSize > pbData + dwLength)
			return false;
		pbTemp += pFile->uiFileSize;
		uiCurrentByte += static_cast<unsigned int>(detailStride);

		pFile=(C4JStorage::DLC_FILE_DETAILS *)&pbData[uiCurrentByte];

	}

	return true;
}

int DLCAudioFile::GetCountofType(EAudioType eType)
{
	return m_parameters[eType].size();
}


wstring &DLCAudioFile::GetSoundName(int iIndex)
{
	int iWorldType=e_AudioType_Overworld;
	while(iIndex>=m_parameters[iWorldType].size())
	{
		iIndex-=m_parameters[iWorldType].size();
		iWorldType++;
	}
	return m_parameters[iWorldType].at(iIndex);
}