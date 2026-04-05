#pragma once

#include "Common/Leaderboards/LeaderboardManager.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class WindowsLeaderboardManager : public LeaderboardManager
{
public:
	WindowsLeaderboardManager();
	virtual ~WindowsLeaderboardManager();

	virtual void Tick();

	virtual bool OpenSession();
	virtual void CloseSession();
	virtual void DeleteSession();

	virtual bool WriteStats(unsigned int viewCount, ViewIn views);

	virtual bool ReadStats_Friends(LeaderboardReadListener *callback, int difficulty, EStatsType type, PlayerUID myUID, unsigned int startIndex, unsigned int readCount);
	virtual bool ReadStats_MyScore(LeaderboardReadListener *callback, int difficulty, EStatsType type, PlayerUID myUID, unsigned int readCount);
	virtual bool ReadStats_TopRank(LeaderboardReadListener *callback, int difficulty, EStatsType type, unsigned int startIndex, unsigned int readCount);

	virtual void FlushStats();
	virtual void CancelOperation();
	virtual bool isIdle();

private:
	void StartReadJob(EFilterMode filter);
	void StartWriteJob(std::vector<RegisterScore> scores);
	void DeliverCompletedWork();
	void DiscardPendingReadDelivery();

	static unsigned int ColumnCountForType(EStatsType type);

	int m_openSessions;

	std::mutex m_queueMutex;
	std::vector<RegisterScore> m_pendingWrites;

	std::atomic<bool> m_workerBusy;
	std::atomic<bool> m_cancelRequested;
	std::thread m_workerThread;

	std::mutex m_deliveryMutex;
	bool m_hasDelivery;
	eStatsReturn m_deliveryReturn;
	ReadScore *m_deliveryScores;
	unsigned int m_deliveryNumScores;
	LeaderboardReadListener *m_deliveryListener;

	std::string m_titleId;
	std::string m_cloudScriptLbColumnsFn;
	std::string m_sessionTicket;
	std::string m_playFabId;
	std::mutex m_authMutex;

	void TrySyncTitleDisplayName();
	bool PostPlayFab(const char *path, const std::string &jsonBody, std::string &outResponseUtf8, std::string &err);

public:
	bool PlayFabEnabled() const { return !m_titleId.empty(); }
	bool EnsureLoggedIn(std::string &err);
	/** ExecuteCloudScript with session ticket; on success copies FunctionResult object JSON into outFunctionResultJson. */
	bool ExecuteCloudScript(const char *functionName, const std::string &functionParameterJson,
		std::string &outFunctionResultJson, std::string &err);
};
