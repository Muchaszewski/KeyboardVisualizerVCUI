#include "CorsairKeyboard.h"

const static int led_matrix_c[7][22]
{//Col Pos:   0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15   16   17   18   19   20   21
	/*Row 0*/   { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 137, 8,   255, 255, 20,  255, 255 },
	/*Row 1*/   { 255, 0,   255, 12,  24,  36,  48,  64,  76,  88,  100, 112, 128, 140, 6,   18,  30,  42,  32,  44,  56,  72  },
	/*Row 2*/   { 255, 1,   13,  25,  37,  49,  65,  77,  89,  101, 113, 129, 141, 7,   31,  54,  70,  82,  84,  96,  108, 120 },
	/*Row 3*/   { 255, 2,   14,  26,  38,  50,  66,  78,  90,  102, 114, 130, 142,  94,  106, 43,  55,  71,  9,   21,  33,  136 },
	/*Row 4*/   { 255, 3,   15,  27,  39,  51,  67,  79,  91,  103, 115, 131, 143,  255, 134, 255, 255, 255, 57,  73,  85,  136 },
	/*Row 5*/   { 255, 4,   255, 28,  40,  52,  68,  80,  92,  104, 116, 132, 144, 255, 83,  255, 107, 255, 97,  109, 121, 148 },
	/*Row 6*/   { 255, 5,   17,  29,  255, 255, 255, 53,  255, 255, 255, 93,  105, 117, 95,  119, 135, 147, 255, 137, 149, 255 }
};

static bool init_ok = TRUE;

//Corsair Keyboard index lists
int CorsairKeyboardXIndex[22];
int CorsairKeyboardYIndex[7];

CorsairKeyboard::CorsairKeyboard()
{
}


CorsairKeyboard::~CorsairKeyboard()
{
}


#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

//==================================================================================================
// Code by http://www.reddit.com/user/chrisgzy
//==================================================================================================
bool IsMatchingDevice(wchar_t *pDeviceID, unsigned int uiVID, unsigned int uiPID, unsigned int uiMI)
{
	unsigned int uiLocalVID = 0, uiLocalPID = 0, uiLocalMI = 0;

	LPWSTR pszNextToken = 0;
	LPWSTR pszToken = wcstok(pDeviceID, L"\\#&");
	while (pszToken)
	{
		std::wstring tokenStr(pszToken);
		if (tokenStr.find(L"VID_", 0, 4) != std::wstring::npos)
		{
			std::wistringstream iss(tokenStr.substr(4));
			iss >> std::hex >> uiLocalVID;
		}
		else if (tokenStr.find(L"PID_", 0, 4) != std::wstring::npos)
		{
			std::wistringstream iss(tokenStr.substr(4));
			iss >> std::hex >> uiLocalPID;
		}
		else if (tokenStr.find(L"MI_", 0, 3) != std::wstring::npos)
		{
			std::wistringstream iss(tokenStr.substr(3));
			iss >> std::hex >> uiLocalMI;
		}

		pszToken = wcstok(0, L"\\#&");
	}

	if (uiVID != uiLocalVID || uiPID != uiLocalPID || uiMI != uiLocalMI)
		return false;

	return true;
}

//==================================================================================================
// Code by http://www.reddit.com/user/chrisgzy
//==================================================================================================
HANDLE GetDeviceHandle(unsigned int uiVID, unsigned int uiPID, unsigned int uiMI)
{
	const GUID GUID_DEVINTERFACE_HID = { 0x4D1E55B2L, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 };
	HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_HID, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE)
		return 0;

	HANDLE hReturn = 0;

	SP_DEVINFO_DATA deviceData = { 0 };
	deviceData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (unsigned int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &deviceData); ++i)
	{
		wchar_t wszDeviceID[MAX_DEVICE_ID_LEN];
		if (CM_Get_Device_IDW(deviceData.DevInst, wszDeviceID, MAX_DEVICE_ID_LEN, 0))
			continue;
		//HID\VID_1B1C&PID_1B33&MI_01&Col01

		if (!IsMatchingDevice(wszDeviceID, uiVID, uiPID, uiMI))
			continue;

		SP_INTERFACE_DEVICE_DATA interfaceData = { 0 };
		interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

		if (!SetupDiEnumDeviceInterfaces(hDevInfo, &deviceData, &GUID_DEVINTERFACE_HID, 0, &interfaceData))
			break;

		DWORD dwRequiredSize = 0;
		SetupDiGetDeviceInterfaceDetail(hDevInfo, &interfaceData, 0, 0, &dwRequiredSize, 0);

		SP_INTERFACE_DEVICE_DETAIL_DATA *pData = (SP_INTERFACE_DEVICE_DETAIL_DATA *)new unsigned char[dwRequiredSize];
		pData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);

		if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &interfaceData, pData, dwRequiredSize, 0, 0))
		{
			auto error = GetLastError();
			delete pData;
			break;
		}

		HANDLE hDevice = CreateFile(pData->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
		if (hDevice == INVALID_HANDLE_VALUE)
		{
			auto error = GetLastError();
			delete pData;
			break;
		}

		hReturn = hDevice;
		break;
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	return hReturn;
}

void CorsairKeyboard::send_usb_msg(char * data_pkt)
{
	char usb_pkt[65];
	for (int i = 1; i < 65; i++)
	{
		usb_pkt[i] = data_pkt[i - 1];
	}
	HidD_SetFeature(dev, usb_pkt, 65);
}

int CorsairKeyboard::Initialize()
{
	char data_pkt[5][64] = { 0 };

	//Look for K70 RGB LUX
	dev = GetDeviceHandle(0x1B1C, 0x1B33, 0x2);
	if (dev == NULL)
	{
		//Look for K95 RGB
		dev = GetDeviceHandle(0x1B1C, 0x1B11, 0x3);
		if (dev == NULL)
		{
			//Look for K70 RGB
			dev = GetDeviceHandle(0x1B1C, 0x1B13, 0x3);
			if (dev == NULL)
			{
				init_ok = FALSE;
				return 0;
			}
		}
	}

	for (int x = 0; x < 22; x++)
	{
		CorsairKeyboardXIndex[x] = x * (256 / 22);
	}
	for (int y = 0; y < 7; y++)
	{
		CorsairKeyboardYIndex[y] = (y - 1) * (64 / 6) + (0.5f * (64 / 6));

		if (y == 0)
		{
			CorsairKeyboardYIndex[y] = 0 * (64 / 6) + (0.5f * (64 / 6));
		}
	}
}

bool CorsairKeyboard::SetLEDs(COLORREF pixels[64][256])
{
	char data_pkt[3][5][64] = { 0 };

	for (size_t i = 0; i < 3; i++)
	{
		data_pkt[i][4][2] = i + 1; //Color RGB

		data_pkt[i][0][0] = 0x7F;
		data_pkt[i][0][1] = 0x01;
		data_pkt[i][0][2] = 0x3C;

		data_pkt[i][1][0] = 0x7F;
		data_pkt[i][1][1] = 0x02;
		data_pkt[i][1][2] = 0x3C;

		data_pkt[i][2][0] = 0x7F;
		data_pkt[i][2][1] = 0x03;
		data_pkt[i][2][2] = 0x18;

		data_pkt[i][3][0] = 0x7F;
		data_pkt[i][3][1] = 0x04;
		data_pkt[i][3][2] = 0x24;

		data_pkt[i][4][0] = 0x07;
		data_pkt[i][4][1] = 0x28;
		data_pkt[i][4][3] = 0x03;
		data_pkt[i][4][4] = 0x02;

		for (int x = 0; x < 22; x++)
		{
			for (int y = 0; y < 7; y++)
			{
				int led = led_matrix_c[y][x];

				if (led < 149)
				{
					int resolvedM = floor(led / 64);
					int resolvedm = floor((led % 64)) + 4;

					data_pkt[0][resolvedM][resolvedm] =  (GetRValue(pixels[CorsairKeyboardYIndex[y]][CorsairKeyboardXIndex[x]]));
					data_pkt[1][resolvedM][resolvedm] =  (GetGValue(pixels[CorsairKeyboardYIndex[y]][CorsairKeyboardXIndex[x]]));
					data_pkt[2][resolvedM][resolvedm] =  (GetBValue(pixels[CorsairKeyboardYIndex[y]][CorsairKeyboardXIndex[x]]));

				}
			}
		}

		send_usb_msg(data_pkt[i][0]);
		send_usb_msg(data_pkt[i][1]);
		send_usb_msg(data_pkt[i][2]);
		send_usb_msg(data_pkt[i][3]);
		send_usb_msg(data_pkt[i][4]);
	}
	Sleep(30);



	return init_ok;
}