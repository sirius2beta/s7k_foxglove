// Teledyne Marine

// Use and Disclosure of Data
//
// EU Uncontrolled Technology: Information contained herein is uncontrolled
// under the E.U. Regulation (EC) No 428/2009.
// However, export, reexport or diversion contrary to law is prohibited.


#include "stdafx.h"
#include "Example7k.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <cmath>


namespace
{
	const char NAME_START[] = "<Name";
	const char DEVID_START[] = "deviceid=\"";
	const char ENUM_START[] = "enumerator=\"";
	const char SONARTYPE_START[] = "<SonarType";
	const char TYPE_START[] = "type=\"";
	const char TYPE_SBES[] = "sbes";

#pragma pack(push, 1)
	struct R7500_1050
	{
		R7500 r7500;
		u32 record_type;
	};

	struct R7500_1051
	{
		R7500 r7500;
		u32 record_count;
		u32 record_types[2];
	};

	struct R7500_10007
	{
		R7500 r7500;
		R7500sub10007 r7500sub10007;
	};
#pragma pack(pop)
}


// The example connects to the sonar,
// request the XML configuration record (7001),
// subscribes to the raw detections record (7027) and image data (7011),
// and prints out the last detection and center pixel of each ping

Example7k::Example7k(): sound_velocity(1500), frequency(400000)
{

}

void Example7k::Run(const char* host)
{
    std::cout<<"run";
    wrapper = new FoxgloveWrapper();
    wrapper->start_worker();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!ConnectToCenter(host))
        return;

    RequestConfig();
	ReceiveLoop();
}


bool Example7k::ConnectToCenter(const char* host)
{
	return m_socket.Create() && m_socket.Connect(host);
}


void Example7k::RequestConfig()
{
	R7500_1050 r7500_1050 = {};
	r7500_1050.r7500.rc_id = 1050;  // Single Record Request
	r7500_1050.record_type = 7001;  // Configuration

	m_socket.SendRecord(7000, 0, 7500, &r7500_1050, sizeof(r7500_1050));
}

void Example7k::SetSbesSonarModeManual()
{
	R7500_10007 r7500_10007 = {};
	r7500_10007.r7500.rc_id = 10007;
	r7500_10007.r7500sub10007.system_state = HYDROGRAPHIC;
	r7500_10007.r7500sub10007.operation_mode = HYDROGRAPHIC_MANUAL;

	m_socket.SendRecord(7000, 0, 7500, &r7500_10007, sizeof(r7500_10007));
}

bool Example7k::ReceiveLoop()
{
	std::vector<u8> recordData;
	while (m_socket.ReceiveRecord(recordData))
	{
		RECORD_FRAME rf = {};
		if (!GetFrame(recordData, rf))
			break;

		size_t dataSize = 0;
		const u8* pData = GetDataPtr(recordData, dataSize);

		switch (rf.record_type)
		{
        case 7000:
            HandleSonarSettings(pData, dataSize);
		case 7001:
			HandleConfig(pData, dataSize);
			break;

		case 7011:
			HandleImage(pData, dataSize);
			break;

		case 7027:
			HandleDetections(pData, dataSize);
			break;

		case 10018:
			HandleSbesEchogram(pData, dataSize);
			break;
		}
	}

	return true;
}

bool Example7k::HandleSonarSettings(const u8* pData, size_t dataSize)
{
    size_t idx = 0;
    R7000 r7000;
    if (idx + sizeof(r7000) > dataSize)
        return false;
    std::memcpy(&r7000, pData + idx, sizeof(r7000));
    idx += sizeof(R7000);
    sound_velocity = r7000.sound_velocity;
    frequency = r7000.frequency;
    return true;
}

// Get device identifier and enumerator from the XML configuration,
// and subscribe to the raw detections (7027) record for each system
bool Example7k::HandleConfig(const u8* pData, size_t dataSize)
{
	size_t idx = 0;

	R7001 r7001;
	if (idx + sizeof(r7001) > dataSize)
		return false;
	std::memcpy(&r7001, pData + idx, sizeof(r7001));
	idx += sizeof(R7001);

	for (u32 i = 0; i < r7001.systems; ++i)
	{
		R7001MODULEINFO r7001Info;
		if (idx + sizeof(r7001Info) > dataSize)
			return false;
		std::memcpy(&r7001Info, pData + idx, sizeof(r7001Info));
		idx += sizeof(r7001Info);

		size_t infoLen = r7001Info.info_length;

		if (idx + infoLen > dataSize)
			return false;
		const char* pInfo = reinterpret_cast<const char*>(pData + idx);
		idx += infoLen;

		if (infoLen == 0 || pInfo[infoLen - 1] != '\0')
			return false;

		// Get device id and enum from XML
		const char* pName = std::strstr(pInfo, NAME_START);
		if (!pName)
			continue;

		const char* pId = std::strstr(pName, DEVID_START);
		if (!pId)
			continue;
		u32 deviceId = (u32) std::atoi(pId + sizeof(DEVID_START) - 1);

		const char* pEnum = std::strstr(pName, ENUM_START);
		if (!pEnum)
			continue;
		u16 enumerator = (u16) std::atoi(pEnum + sizeof(ENUM_START) - 1);

		// Get sonar type from XML
		const char* pSonarType = std::strstr(pInfo, SONARTYPE_START);
		if (!pSonarType)
			continue;
		const char* pType = std::strstr(pSonarType, TYPE_START);
		if (!pType)
			continue;

		// Extract type of sonar
		pType = pType + sizeof(TYPE_START) - 1;
		const char* pTypeEnd = std::strstr(pType, "\"");
		if (!pTypeEnd)
			continue;
		size_t count = (size_t)(pTypeEnd - pType);
		if (std::strncmp(pType, TYPE_SBES, count) == 0)
		{
			// This is a SBES sonar
			SubscribeSbes(deviceId, 1); // Subscribe for Echogram data on Channel 1
			SubscribeSbes(deviceId, 2); // Subscribe for Echogram data on Channel 2

			// Try to set sonar mode to manual
			SetSbesSonarModeManual();
		}
		else
		{
			// MUST be MBES sonar ("0" for bathy, "1" for forward looker)
			Subscribe(deviceId, enumerator);
		}
	}

	return true;
}


void Example7k::Subscribe(u32 deviceId, u16 enumerator)
{
	R7500_1051 r7500_1051 = {};
	r7500_1051.r7500.rc_id = 1051;  // Record Subscription
	r7500_1051.record_count = 2;
	r7500_1051.record_types[0] = 7027;  // Raw Detections
	r7500_1051.record_types[1] = 7011;  // Image Data

	m_socket.SendRecord(deviceId, enumerator, 7500, &r7500_1051,
		sizeof(r7500_1051));
}

void Example7k::SubscribeSbes(u32 deviceId, u16 enumerator)
{
	R7500_1051 r7500_1051 = {};
	r7500_1051.r7500.rc_id = 1051;  // Record Subscription
	r7500_1051.record_count = 1;
	r7500_1051.record_types[0] = 10018;  //Echogram data
	

	m_socket.SendRecord(deviceId, enumerator, 7500, &r7500_1051,
		sizeof(r7500_1051));
}


bool Example7k::HandleDetections(const u8* pData, size_t dataSize)
{
	size_t idx = 0;

	// The size of the 7027 header part is fixed
	R7027 r7027;
	if (idx + sizeof(r7027) > dataSize)
		return false;
	std::memcpy(&r7027, pData + idx, sizeof(r7027));
	idx += sizeof(r7027);

	// The size of the 7027 data parts is specified in the header
	// so that fields may be added in the future
	// We have to be able to deal with shorter (older) versions,
	// and longer (newer) versions
	size_t r7027DataSize = r7027.data_size;
	size_t minSize = (std::min)(sizeof(R7027Data), r7027DataSize);

	R7027Data r7027Data = {};
    std::vector<float> points;
    float sv = 1500;
	for (u32 i = 0; i < r7027.detections; ++i)
	{
		if (idx + r7027DataSize > dataSize)
			return false;
		const u8* p7027Data = pData + idx;
		idx += r7027DataSize;

		std::memcpy(&r7027Data, p7027Data, minSize);
        float range = sound_velocity*r7027Data.detection/(2*frequency);
        float x = range*std::sin(r7027Data.rx_angle);
        float y = 0;
        float z = -range*std::cos(r7027Data.rx_angle);
        float intensity = r7027Data.signal_strength;
        points.push_back(x);          // x
        points.push_back(y); // y
        points.push_back(z);
        points.push_back(intensity);            // z
	}

	// Print last detection of each ping

    wrapper->setPointCloud(points);
	return true;
}

bool Example7k::HandleSbesEchogram(const u8* pData, size_t dataSize)
{
	size_t idx = 0;

	// The size of the 10018 header part is fixed
	R10018 r10018;
	if (idx + sizeof(r10018) > dataSize)
		return false;
	std::memcpy(&r10018, pData + idx, sizeof(r10018));
	idx += sizeof(r10018);

	if (r10018.bits_per_sample != 32)
		return false;
	
	std::vector <u32> r10018data(r10018.sample_count); // Vector for echogram data
	std::memcpy(r10018data.data(), pData + idx, r10018.sample_count);

	// Print ping number and size
	std::printf("Ping %u, number of samples %u\n",
		(unsigned)r10018.ping_number,
		(unsigned)r10018.sample_count);

	return true;
}

bool Example7k::HandleImage(const u8* pData, size_t dataSize)
{
	size_t idx = 0;

	R7011 r7011 = {};
	if (idx + sizeof(r7011) > dataSize)
		return false;
	std::memcpy(&r7011, pData + idx, sizeof(r7011));
	idx += sizeof(r7011);

	size_t imageDataSize = r7011.width * r7011.height * r7011.color_depth;
	if (idx + imageDataSize > dataSize)
		return false;
	const u8* pImageData = pData + idx;

	if (r7011.color_depth != 1)
		return false;  // not supported

	// Print value of the center pixel
	size_t beamNo = r7011.width / 2;
	size_t sampleNo = r7011.height / 2;
	u8 pixel = pImageData[beamNo + r7011.width * sampleNo];
	std::printf("Ping %u, image center pixel = %u\n",
		(unsigned) r7011.ping_number,
		(unsigned) pixel);

	return true;
}
