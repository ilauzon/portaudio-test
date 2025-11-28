#include "main_frame.h"

#include "speaker_panel.h"
#include "../start.h"
#include "../portaudio_listener.h"

#include <portaudio.h>
#include <thread>

extern paTestData gData;
void SetOutputDeviceIndex(int index);  // from portaudio_listener.cpp

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_TIMER(MyFrame::ID_TimerRefresh,    MyFrame::OnTimer)
    EVT_MENU(wxID_EXIT,                    MyFrame::OnExit)
    EVT_MENU(wxID_ABOUT,                   MyFrame::OnAbout)
    EVT_MENU(MyFrame::ID_Hello,            MyFrame::OnHello)
    EVT_CHOICE(MyFrame::ID_DeviceChoice,   MyFrame::OnDeviceChoice)
    EVT_CHOICE(MyFrame::ID_ModeChoice,     MyFrame::OnModeChoice)
    EVT_BUTTON(MyFrame::ID_StartAudio,     MyFrame::OnStartAudio)
    EVT_BUTTON(MyFrame::ID_ResetPositions, MyFrame::OnResetPositions)
wxEND_EVENT_TABLE()

MyFrame::MyFrame(bool interactiveMode)
    : wxFrame(nullptr, wxID_ANY, "PortAudio + wxWidgets (Live Volumes)",
              wxDefaultPosition, wxSize(980, 600))
    , m_interactiveMode(interactiveMode)
    , m_timer(this, ID_TimerRefresh)
{
    // Menus
    wxMenu* menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&Hello...\tCtrl-H");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);

    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);

    // Status bar
    CreateStatusBar();
    if (m_interactiveMode)
        SetStatusText("Interactive mode: drag listener & speakers. Speakers movable in both modes.");
    else
        SetStatusText("Follow stdin: listener locked; speakers still movable. Speakers movable in both modes.");

    // Layout
    wxPanel* rootPanel   = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Top row: device + mode
    wxBoxSizer* deviceSizer = new wxBoxSizer(wxHORIZONTAL);

    deviceSizer->Add(new wxStaticText(rootPanel, wxID_ANY, "Output device:"),
                     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_deviceChoice = new wxChoice(rootPanel, ID_DeviceChoice);
    deviceSizer->Add(m_deviceChoice, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 15);

    deviceSizer->Add(new wxStaticText(rootPanel, wxID_ANY, "Mode:"),
                     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_modeChoice = new wxChoice(rootPanel, ID_ModeChoice);
    m_modeChoice->Append("Interactive (drag head)");
    m_modeChoice->Append("Follow stdin (head locked)");
    m_modeChoice->SetSelection(m_interactiveMode ? 0 : 1);
    deviceSizer->Add(m_modeChoice, 0, wxALIGN_CENTER_VERTICAL);

    mainSizer->Add(deviceSizer, 0, wxEXPAND | wxALL, 10);

    // Middle: panel
    m_panel = new SpeakerPanel(rootPanel, &gData, m_interactiveMode);
    mainSizer->Add(m_panel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    // Bottom row: Reset (left) + Start (right)
    wxBoxSizer* bottomSizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton* resetBtn = new wxButton(rootPanel, ID_ResetPositions, "Reset positions");
    bottomSizer->Add(resetBtn, 0, wxALIGN_LEFT | wxRIGHT, 10);

    bottomSizer->AddStretchSpacer(1);

    wxButton* startBtn = new wxButton(rootPanel, ID_StartAudio, "Start Audio");
    bottomSizer->Add(startBtn, 0, wxALIGN_RIGHT);

    mainSizer->Add(bottomSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    rootPanel->SetSizer(mainSizer);

    m_timer.Start(50);

    // PortAudio device enumeration
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        wxLogError("PortAudio error: %s", Pa_GetErrorText(err));
        return;
    }

    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
    {
        wxLogError("PortAudio error: Pa_GetDeviceCount returned %d", numDevices);
        return;
    }

    int defaultOut = Pa_GetDefaultOutputDevice();
    int choiceIndexForDefault = -1;

    for (int i = 0; i < numDevices; ++i)
    {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;

        if (info->maxOutputChannels <= 0) continue;

        const PaHostApiInfo* hostInfo = Pa_GetHostApiInfo(info->hostApi);

        wxString label;
        label.Printf("%d: %s (%s)",
                     i,
                     info->name ? info->name : "Unknown",
                     hostInfo ? hostInfo->name : "Unknown API");

        int currentChoiceIndex = (int)m_outputDeviceIndices.size();
        m_deviceChoice->Append(label);
        m_outputDeviceIndices.push_back(i);

        if (i == defaultOut)
            choiceIndexForDefault = currentChoiceIndex;
    }

    if (!m_outputDeviceIndices.empty())
    {
        int toSelect = (choiceIndexForDefault >= 0)
                       ? choiceIndexForDefault
                       : 0;
        m_deviceChoice->SetSelection(toSelect);
        SetOutputDeviceIndex(m_outputDeviceIndices[toSelect]);
    }

    if (m_panel)
        m_panel->SetInteractive(m_interactiveMode);
}

void MyFrame::OnStartAudio(wxCommandEvent &event)
{
    std::thread audioThread([]() {
        int result = start();
        wxTheApp->CallAfter([result]() {
            wxFrame* top = dynamic_cast<wxFrame*>(wxTheApp->GetTopWindow());
            if (top)
            {
                top->SetStatusText(result == 0 ? "Audio finished." : "Audio error.");
            }
        });
    });
    audioThread.detach();

    SetStatusText("Audio running...");
}

void MyFrame::OnExit(wxCommandEvent &event)
{
    Close(true);
}

void MyFrame::OnAbout(wxCommandEvent &event)
{
    wxMessageBox("PortAudio + wxWidgets example with live volume visuals.",
                 "About", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnHello(wxCommandEvent &event)
{
    wxLogMessage("Hello from wxWidgets!");
}

void MyFrame::OnTimer(wxTimerEvent &event)
{
    if (m_panel)
    {
        m_panel->Refresh(false);
    }
}

void MyFrame::OnDeviceChoice(wxCommandEvent &event)
{
    int selection = m_deviceChoice->GetSelection();
    if (selection < 0 || selection >= (int)m_outputDeviceIndices.size())
        return;

    int paDeviceIndex = m_outputDeviceIndices[selection];
    SetOutputDeviceIndex(paDeviceIndex);
}

void MyFrame::OnModeChoice(wxCommandEvent &event)
{
    int sel = m_modeChoice ? m_modeChoice->GetSelection() : 0;
    bool interactive = (sel == 0);   // true = can drag listener

    m_interactiveMode = interactive;

    if (m_panel)
    {
        // Update listener drag ability
        m_panel->SetInteractive(m_interactiveMode);

        // When switching to "head locked" / stdin mode, auto-reset positions
        if (!m_interactiveMode)
        {
            m_panel->ResetPositions();
        }
    }

    if (m_interactiveMode)
        SetStatusText("Interactive mode: drag listener & speakers. Speakers movable in both modes.");
    else
        SetStatusText("Follow stdin: listener locked; speakers still movable. Speakers movable in both modes.");
}

void MyFrame::OnResetPositions(wxCommandEvent &event)
{
    if (m_panel)
    {
        m_panel->ResetPositions();
        SetStatusText("Positions reset to defaults.");
    }
}