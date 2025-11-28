#pragma once

#include <wx/wx.h>
#include <wx/timer.h>
#include <vector>

class SpeakerPanel;

class MyFrame : public wxFrame
{
public:
    MyFrame(bool interactiveMode);

private:
    enum
    {
        ID_Hello = 1,
        ID_TimerRefresh,
        ID_DeviceChoice,
        ID_StartAudio,
        ID_ModeChoice,
        ID_ResetPositions   // <-- new
    };

    bool m_interactiveMode = true;

    void OnStartAudio(wxCommandEvent &event);
    void OnExit(wxCommandEvent &event);
    void OnAbout(wxCommandEvent &event);
    void OnHello(wxCommandEvent &event);
    void OnTimer(wxTimerEvent &event);
    void OnDeviceChoice(wxCommandEvent &event);
    void OnModeChoice(wxCommandEvent &event);
    void OnResetPositions(wxCommandEvent &event);   // <-- new

    wxTimer       m_timer;
    wxChoice*     m_deviceChoice = nullptr;
    wxChoice*     m_modeChoice   = nullptr;
    SpeakerPanel* m_panel        = nullptr;

    std::vector<int> m_outputDeviceIndices;

    wxDECLARE_EVENT_TABLE();
};