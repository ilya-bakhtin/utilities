#pragma once

class ThreadEnumerator
{
friend class TorSupervisor;
	virtual bool OnThread(DWORD threadID) = 0;
};

class TorSupervisor
{
public:
	TorSupervisor(void);
	virtual ~TorSupervisor(void);

	bool isTorRunning(bool *in_window = NULL);
	void terminateTor();

	void EnumProcThreads(LPCTSTR proc_name, ThreadEnumerator *e);
};
