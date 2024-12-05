#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream> 
#include <cstring> 
#include <cmath>

#include "HCNetSDK.h"
#include "json.hpp"
#include "curl/curl.h"

#pragma warning(disable : 4996)
using json = nlohmann::json;

char IPAddress[16] = "127.0.0.1";   // Default value
DWORD Port = 80;				    // Default value
std::string URL;

void CALLBACK MyAlarmCallBack(LONG lCommand, NET_DVR_ALARMER* pAlarmer, char* pAlarmInfo, DWORD dwBufLen, void* pUser)
{
	std::stringstream ss;
	ss << "0x" << std::hex << std::uppercase << lCommand;
	std::string hexCommand = ss.str();
	switch (lCommand)
	{
		case COMM_ALARM_TPS_STATISTICS: {
			NET_DVR_TPS_STATISTICS_INFO* pTpsRealTimeInfo = (NET_DVR_TPS_STATISTICS_INFO*)pAlarmInfo;
			NET_DVR_TIME_V30* struStartTime = &pTpsRealTimeInfo->struTPSStatisticsInfo.struStartTime;
			
			int totalLaneNum=0;

			std::string deviceIP(pAlarmer->sDeviceIP);
			std::ostringstream oss;
			oss << std::setfill('0') << std::setw(4) << (int)struStartTime->wYear << "-"
				<< std::setw(2) << (int)struStartTime->byMonth << "-"
				<< std::setw(2) << (int)struStartTime->byDay << "T"
				<< std::setw(2) << (int)struStartTime->byHour << ":"
				<< std::setw(2) << (int)struStartTime->byMinute << ":"
				<< std::setw(2) << (int)struStartTime->bySecond << "."
				<< std::setw(3) << (int)struStartTime->wMilliSec
				<< "+05:00"; 

			// Create json data
			json laneInfoArray = json::array();
			for (int i = 0; i < sizeof(pTpsRealTimeInfo->struTPSStatisticsInfo.struLaneParam) / sizeof(pTpsRealTimeInfo->struTPSStatisticsInfo.struLaneParam[0]); i++) {
				NET_DVR_TPS_LANE_PARAM* pTpsLaneParam = &pTpsRealTimeInfo->struTPSStatisticsInfo.struLaneParam[i];
				if (!pTpsLaneParam->byLane == 0x00) {
					totalLaneNum++;
					laneInfoArray.push_back({
							{"averageQueueLen", pTpsLaneParam->byQueueLen},
							{"aversgeSpeed", pTpsLaneParam->bySpeed},
							{"channelizationLaneNo", 1},
							{"headInterval", 0},
							{"headTimeInterval", pTpsLaneParam->dwTimeHeadway},
							{"heavyVehicleNum", pTpsLaneParam->dwHeavyVehicle},
							{"laneNo", pTpsLaneParam->byLane},
							{"midsizeCarNum", pTpsLaneParam->dwMidVehicle},
							{"smallCarNum", pTpsLaneParam->dwLightVehicle},
							{"spaceOccupyRation", int(round((pTpsLaneParam->fSpaceOccupyRation)*100))},
							{"timeOccupyRation", int(round((pTpsLaneParam->fTimeOccupyRation)*100))}
					});
				}
			}
			json j;
			j["Target"] = json::array({
				{
					{"TargetInfo", {
						{"LaneInfo", laneInfoArray},
						{"recognition", "TPS"},
						{"startTime", oss.str()},
						{"totalLaneNum", NULL}
					}},
					{"recognitionType", "vehicle"}
				}
			});
			j["eventDescription"] = "traffic pass statistics";
			j["eventState"] = "active";
			j["eventType"] = "TPS";
			j["ipAddress"] = deviceIP;
			j["Target"][0]["TargetInfo"]["totalLaneNum"] = totalLaneNum;

			// HTTP POST REQUEST->>>
			CURL* curl;
			CURLcode res;
			std::string json_data = j.dump();
			curl = curl_easy_init();
			if (curl) {
				// URL in POST request
				curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
				// POST method
				curl_easy_setopt(curl, CURLOPT_POST, 1L);
				// Send JSON data
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
				// Cleanup headers
				struct curl_slist* headers = nullptr;
				headers = curl_slist_append(headers, "Content-Type: application/json");
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
				// Apply request
				res = curl_easy_perform(curl);
				// Check response
				if (res != CURLE_OK) {
					std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
				}
				// Cleanup
				curl_slist_free_all(headers);
				curl_easy_cleanup(curl);
			}
			std::cout << j.dump(4) << std::endl;
		}break;
		default: {
			std::cout<<"Alarm recived. Command : %d"<< hexCommand<<std::endl;
		}break;
	}
}
int main() 
{
	char logPath[9] = "./SDKLog";
	LONG listenHandle;
	std::ifstream configFile("config.txt");
	if (!configFile.is_open()) {
		std::cerr << "Error open file config.txt.\n" << std::endl;
		Sleep(5000);
		main();
	}
	std::string line;
	while (std::getline(configFile, line)) {
		if (line.find("IPAddress=") == 0) {
			std::string ip = line.substr(10); 
			std::strcpy(IPAddress, ip.c_str());
		}
		else if (line.find("Port=") == 0) {
			std::string portStr = line.substr(5); 
			Port = std::stoi(portStr); 
		}else if (line.find("URL=") == 0) {
			URL = line.substr(4); 
		}
	}
	configFile.close();

	if (NET_DVR_Init()) {
		printf("Init Successfuly\n");
	}
	else {
		printf("NET_DVR_Init error\n");
		Sleep(5000);
		main();
	}
	NET_DVR_SetLogToFile(3, logPath, TRUE);
	listenHandle = NET_DVR_StartListen_V30(IPAddress,Port,MyAlarmCallBack,NULL);
	if (listenHandle < 0) {
		std::cerr << "Error listening, code : "<< NET_DVR_GetLastError() << std::endl;
		NET_DVR_Cleanup();
		Sleep(5000);
		main();
	}
	std::cout<<"Listening Started : IP = "<<IPAddress<<" PORT = "<<Port<<std::endl;
	std::cin.get();
	NET_DVR_StopListen();
	NET_DVR_Cleanup();
	main();
}