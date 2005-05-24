/***************************************************************************
 *            ComparePanel.cc
 *
 *  Tue Mar  1 00:24:16 2005
 *  Copyright  2005  Chris Wilson
 *  anjuta@qwirx.com
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

// #include <wx/arrstr.h>
#include <wx/dir.h>
#include <wx/filename.h>

#include "ComparePanel.h"

CompareThread::CompareThread(
	ClientConfig* pConfig, 
	Listener* pListener)
: wxThread(wxTHREAD_JOINABLE),
  mpConfig(pConfig),
  mpListener(pListener)
{
	mLastError = ERR_NONE;
	mCurrentState = WS_IDLE;
	Create();
	Run();
}

CompareThread::~CompareThread() 
{
	// Delete();
	// Wait();
}

void* CompareThread::Entry()
{
	wxLogDebug("Compare thread starting");
	
	try 
	{
		mDb.setDatabase("test.db");
		mpDs = mDb.CreateDataset();
		mDb.connect();
		
		mpDs->exec("create table foo(bar int4)");
		mpDs->exec("drop table foo");
	}
	catch(...)
	{
		wxLogError("Error connecting to database");
	}		
		
	// unlocked use of mCurrentState, might possibly be dangerous?
	while (mCurrentState != WS_SHUTDOWN)
	{
		WorkerState PrevWorkerState;
		
		{
			wxMutexLocker lock(mMutex);
			PrevWorkerState = mCurrentState;
			mLastError = ERR_NONE;
		}
		
		// wxLogDebug("Current worker state: %s", GetStateStr(PrevState));
		
		WorkerError err = ERR_UNKNOWN;
		
		try {
			switch (PrevWorkerState)
			{
				case WS_IDLE:
					err = OnIdle(); break;
				case WS_LOCALSCAN:
					err = OnStartLocalScan(); break;
				case WS_STOPSCAN:
					// scan already stopped, or we wouldn't be here
					err = OnIdle();
					break;
				case WS_SHUTDOWN:
					err = ERR_NONE;
					break;
				default:
					wxLogError("Unknown compare thread state %d", 
							   PrevWorkerState);
			}
		} catch (...) {
			wxLogError("Caught exception");
		}

		if (TestDestroy()) {
			wxMutexLocker lock(mMutex);
			mCurrentState = WS_SHUTDOWN;
			PrevWorkerState = mCurrentState;
			break;
		}
		
		{
			wxMutexLocker lock(mMutex);
			mLastError = err;
		}
		
		if (mpListener)
		{
			if (mCurrentState != PrevWorkerState) {
				mpListener->NotifyStateChange();
				wxLogDebug("Worker state change: '%d' to '%d'",
					PrevWorkerState, mCurrentState);
			}
			mpListener->NotifyError();
			if (mLastError != ERR_NONE) {
				wxLogDebug("Worker error: '%d'", err);
			}
		}
			
		{
			wxMutexLocker lock(mMutex);

			if (mCurrentState == WS_IDLE)
				Sleep(1000);

			if (mCurrentState == WS_STOPSCAN)
				mCurrentState = WS_IDLE;
		}
	}
	
	wxLogDebug("Compare thread shutdown");
	return NULL;
}

CompareThread::WorkerError CompareThread::OnStartLocalScan()
{
	std::auto_ptr<ClientConfig> apConf;
	
	{
		wxMutexLocker lock(mMutex);
		if (mCurrentState != WS_LOCALSCAN)
			return ERR_INTERRUPTED;
		
		assert(mapConfigCopy.get());
		ClientConfig* pLocalCopy = new ClientConfig(*(mapConfigCopy.get()));
		apConf.reset(pLocalCopy);
	}
	
	const std::vector<Location*>& rLocs(apConf->GetLocations());
	for (size_t i = 0; i < rLocs.size(); i++)
	{
		Location* pLoc = rLocs[i];
		wxString path = pLoc->GetPath();
		ScanLocalDir(path, apConf.get());
	}
	
	{
		wxMutexLocker lock(mMutex);
		if (mCurrentState != WS_LOCALSCAN)
			return ERR_INTERRUPTED;
		
		mCurrentState = WS_IDLE;
		return ERR_NONE;
	}
}

void CompareThread::ScanLocalDir(const wxString& rPath, ClientConfig* pConfig)
{
	if (wxFileName::DirExists(rPath))
	{
		wxLogDebug("Directory: %s", rPath.c_str());
		wxArrayString entries;
		wxDir::GetAllFiles(rPath, &entries);
		for (size_t i = 0; i < entries.GetCount(); i++)
		{
			wxString entry = entries.Item(i);
			wxFileName fn(rPath + "/" + entry);
			ScanLocalDir(fn.GetFullPath(), pConfig);
		}
	}
	else if (wxFileName::FileExists(rPath))
	{
		wxLogDebug("File: %s", rPath.c_str());
	}
	else 
	{
		wxLogDebug("No such file or directory: %s", rPath.c_str());
	}
}

CompareThread::WorkerError CompareThread::OnIdle()
{ return ERR_NONE; }

BEGIN_EVENT_TABLE(ComparePanel, wxPanel)
EVT_BUTTON(ID_Compare_Button, ComparePanel::OnCompareClick)
END_EVENT_TABLE()

ComparePanel::ComparePanel(
	ClientConfig *pConfig,
	ServerConnection* pServerConnection,
	wxWindow* parent, 
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name)
	: wxPanel(parent, id, pos, size, style, name)
{
	mpConfig = pConfig;
	mpServerConnection = pServerConnection;
	
	wxSizer* pMainSizer = new wxBoxSizer( wxVERTICAL );
	
	wxSizer* pParamSizer = new wxGridSizer(2, 4, 4);
	pMainSizer->Add(pParamSizer, 0, wxGROW | wxALL, 8);
	
	mpCompareThreadStateCtrl = new wxTextCtrl(this, -1, "");
	::AddParam(this, "Current state:", mpCompareThreadStateCtrl,
		false, pParamSizer);

	wxSizer* pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
	pMainSizer->Add(pButtonSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	wxButton* pCompareButton = new wxButton(this, ID_Compare_Button,
		"Compare");
	pButtonSizer->Add(pCompareButton);

	mpCompareList = new wxListCtrl(this, ID_Compare_List, wxDefaultPosition,
		wxDefaultSize, wxLC_REPORT | wxSUNKEN_BORDER);
	pMainSizer->Add(mpCompareList, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	SetSizer( pMainSizer );
	pMainSizer->SetSizeHints( this );
		
	mapCompareThread = std::auto_ptr<CompareThread>(
		new CompareThread(pConfig, NULL));
	
	UpdateCompareThreadStateCtrl();
	
	CompareThread::WorkerError err = mapCompareThread->SendStartCompare();
	if (err != CompareThread::ERR_NONE)
	{
		wxLogDebug("Error starting scan: %d", err);
	}
}

void ComparePanel::UpdateCompareThreadStateCtrl()
{
	CompareThread::WorkerState state = mapCompareThread->GetState();
	wxString msg;
	
	switch (state)
	{
		case CompareThread::WS_IDLE:
			msg = "Idle"; break;
		case CompareThread::WS_LOCALSCAN:
			msg = "Scanning local files"; break;
		case CompareThread::WS_STOPSCAN:
			msg = "Waiting for scan to stop"; break;
		case CompareThread::WS_SHUTDOWN:
			msg = "Waiting for thread to die"; break;
		default:
			msg = "Unknown"; break;
	}
	
	mpCompareThreadStateCtrl->SetValue(msg);
}
