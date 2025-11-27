// gui.cpp
//
// wxWidgets GUI with:
// - Device selection (output-only)
// - Room + speakers visualization
// - Live-updating volume sliders (short, wide right triangles with sweep fill)
// - Background audio thread
// - wxTimer to animate the GUI
//

#include "start.h"                 // int start(); void initAudioData();
#include "portaudio_listener.h"    // paTestData, CHANNEL_COUNT, etc.

#include <cstdlib>
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/dcbuffer.h>           // wxAutoBufferedPaintDC
#include <thread>
#include <vector>

#include <portaudio.h>             // For device enumeration

// ============================================================
//   Global audio data (defined in start.cpp)
// ============================================================

extern paTestData gData;

// From portaudio_listener.cpp
void SetOutputDeviceIndex(int index);  // PaDeviceIndex, or paNoDevice for default

// ============================================================
//   Custom panel that draws room + speakers + volume meters
// ============================================================

class SpeakerPanel : public wxPanel
{
public:
    SpeakerPanel(wxWindow* parent, paTestData* data)
        : wxPanel(parent), m_data(data)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &SpeakerPanel::OnPaint, this);
    }

private:
    paTestData* m_data = nullptr;

    // Fixed world->screen mapping: (0,0) is center of panel
    wxPoint worldToScreen(float x, float y, int w, int h, float scale)
    {
        float cx = w * 0.5f;
        float cy = h * 0.5f;

        float sx = cx + x * scale;
        float sy = cy - y * scale; // invert Y so +Y is up

        return wxPoint((int)sx, (int)sy);
    }

    void OnPaint(wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.Clear();

        if (!m_data)
            return;

        int w, h;
        GetClientSize(&w, &h);

        // ----- Fixed coordinate system -----
        float minX = m_data->subjectBounds[0].x;
        float minY = m_data->subjectBounds[0].y;
        float maxX = m_data->subjectBounds[1].x;
        float maxY = m_data->subjectBounds[1].y;

        // Determine maximum extents to keep room within panel with margin
        const int margin = 20;
        float maxAbsX = std::max(std::fabs(minX), std::fabs(maxX));
        float maxAbsY = std::max(std::fabs(minY), std::fabs(maxY));

        if (maxAbsX < 0.001f) maxAbsX = 1.0f;
        if (maxAbsY < 0.001f) maxAbsY = 1.0f;

        float scaleX = (w / 2.0f - margin) / maxAbsX;
        float scaleY = (h / 2.0f - margin) / maxAbsY;
        float scale  = (scaleX < scaleY ? scaleX : scaleY);

        // ----- Draw room rectangle using worldToScreen -----
        dc.SetPen(*wxBLACK_PEN);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        wxPoint topLeft     = worldToScreen(minX, maxY, w, h, scale);
        wxPoint topRight    = worldToScreen(maxX, maxY, w, h, scale);
        wxPoint bottomLeft  = worldToScreen(minX, minY, w, h, scale);
        // wxPoint bottomRight = worldToScreen(maxX, minY, w, h, scale);

        int roomX   = topLeft.x;
        int roomY   = topLeft.y;
        int roomWpx = topRight.x - topLeft.x;
        int roomHpx = bottomLeft.y - topLeft.y;

        dc.DrawRectangle(roomX, roomY, roomWpx, roomHpx);

        // ----- Draw speakers + volume sliders -----
        const int speakerBoxSize = 12;

        // Short & wide sliders
        const int sliderHeight    = 14;  // shorter
        const int sliderMaxWidth  = 50;  // wider
        const int sliderOffsetX   = 20;  // distance from speaker box

        for (int i = 0; i < CHANNEL_COUNT; ++i)
        {
            // Clamp volume between 0 and 1
            float vol = m_data->channelGains[i];
            // if (vol < 0.0f) vol = 0.0f;
            // if (vol > 1.0f) vol = 1.0f;

            Point sp = m_data->speakerPositions[i];
            wxPoint p = worldToScreen(sp.x, sp.y, w, h, scale);

            // Draw speaker box
            int halfBox = speakerBoxSize / 2;
            dc.SetPen(*wxBLACK_PEN);
            dc.SetBrush(*wxLIGHT_GREY_BRUSH);
            dc.DrawRectangle(p.x - halfBox, p.y - halfBox,
                             speakerBoxSize, speakerBoxSize);

            // Slider base anchor (bottom-left corner of the slider)
            int baseX = p.x + halfBox + sliderOffsetX;
            int baseY = p.y + sliderHeight / 2;

            // Background triangle (full size, light grey, NO outline)
            wxPoint A(baseX, baseY);                         // bottom-left (right angle)
            wxPoint B(baseX + sliderMaxWidth, baseY);        // bottom-right
            wxPoint C(baseX, baseY - sliderHeight);          // top-left

            wxPoint bgTri[3] = { A, B, C };

            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(wxColour(220, 220, 220)));
            dc.DrawPolygon(3, bgTri);

            // Filled region for current volume:
            //  A is fixed (bottom-left)
            //  D moves along AB
            //  E moves along the slanted edge CB
            //
            // Param t = vol in [0,1]
            if (vol > 0.0f)
            {
                float t = vol;

                wxPoint D(baseX + (int)(sliderMaxWidth * t), baseY); // on AB
                wxPoint E(
                    C.x + (int)((B.x - C.x) * t),
                    C.y + (int)((B.y - C.y) * t)
                ); // on CB

                wxPoint fillPoly[4] = { A, D, E, C };

                int green = (int)(80 + 175 * vol);
                if (green > 255) green = 255;

                dc.SetPen(*wxTRANSPARENT_PEN);              // NO outline
                dc.SetBrush(wxBrush(wxColour(40, green, 40)));
                dc.DrawPolygon(4, fillPoly);
            }

            // Numeric label beside the slider
            wxString label;
            label.Printf("Ch %d  %.2f", i, vol);
            dc.SetPen(*wxBLACK_PEN);
            dc.DrawText(label,
                        baseX + sliderMaxWidth + 8,
                        baseY - sliderHeight / 2);
        }

        // ----- Draw listener -----
        Point L = m_data->currentListenerPosition;
        wxPoint lp = worldToScreen(L.x, L.y, w, h, scale);

        dc.SetPen(*wxBLUE_PEN);
        dc.SetBrush(*wxBLUE_BRUSH);
        dc.DrawCircle(lp, 6);
        dc.DrawText("Listener", lp.x + 8, lp.y - 6);

        // ----- Draw listener facing direction -----
        float yawRadians = -m_data->listenerYaw * 2.0f * M_PI; // 0..1 → 0..2π

        // Length of the line/cone in world units
        float dirLength = 1.0f; // 1 metre; adjust as needed

        // Compute end point
        float endX = L.x + dirLength * std::sin(yawRadians); // X points right
        float endY = L.y + dirLength * std::cos(yawRadians); // Y points forward

        wxPoint endPoint = worldToScreen(endX, endY, w, h, scale);

        // Draw simple line
        dc.SetPen(wxPen(*wxRED, 2)); // red line for facing direction
        dc.DrawLine(lp, endPoint);

        // Optional: draw a triangular “cone” to indicate field of view
        float coneAngle = 15.0f * (M_PI / 180.0f); // 15° half-angle
        float coneLength = dirLength * 0.8f;       // slightly shorter than line
        wxPoint leftTip = worldToScreen(
            L.x + coneLength * std::sin(yawRadians - coneAngle),
            L.y + coneLength * std::cos(yawRadians - coneAngle),
            w, h, scale
        );
        wxPoint rightTip = worldToScreen(
            L.x + coneLength * std::sin(yawRadians + coneAngle),
            L.y + coneLength * std::cos(yawRadians + coneAngle),
            w, h, scale
        );

        wxPoint conePoly[3] = { lp, leftTip, rightTip };
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColour(255, 0, 0, 80))); // semi-transparent red
        dc.DrawPolygon(3, conePoly);
    }
};


// ============================================================
//   Main window (MyFrame) with device selection + timer
// ============================================================

class MyFrame : public wxFrame
{
public:
    MyFrame();

private:
    void OnStartAudio(wxCommandEvent &event);
    void OnExit(wxCommandEvent &event);
    void OnAbout(wxCommandEvent &event);
    void OnHello(wxCommandEvent &event);
    void OnTimer(wxTimerEvent &event);
    void OnDeviceChoice(wxCommandEvent &event);

    void InitOutputDevices();

    SpeakerPanel* m_panel = nullptr;
    bool m_isPlaying = false;
    wxTimer m_timer;

    wxChoice* m_deviceChoice = nullptr;
    std::vector<int> m_outputDeviceIndices; // PaDeviceIndex values
    int m_selectedDeviceIndex = paNoDevice;
};

enum
{
    ID_Hello = wxID_HIGHEST + 1,
    ID_StartAudio,
    ID_TimerRefresh,
    ID_DeviceChoice
};

MyFrame::MyFrame()
    : wxFrame(nullptr, wxID_ANY, "PortAudio + wxWidgets (Live Volumes)",
              wxDefaultPosition, wxSize(900, 600)),
      m_timer(this, ID_TimerRefresh)
{
    // ----- Menus -----
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

    // ----- Status bar -----
    CreateStatusBar();
    SetStatusText("Ready.");

    // ----- Layout: device row + panel + button -----
    wxPanel* rootPanel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Device selection row
    wxBoxSizer* deviceSizer = new wxBoxSizer(wxHORIZONTAL);
    deviceSizer->Add(new wxStaticText(rootPanel, wxID_ANY, "Output device:"),
                     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_deviceChoice = new wxChoice(rootPanel, ID_DeviceChoice);
    deviceSizer->Add(m_deviceChoice, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

    mainSizer->Add(deviceSizer, 0, wxEXPAND | wxALL, 10);

    // Visualization panel
    m_panel = new SpeakerPanel(rootPanel, &gData);
    mainSizer->Add(m_panel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    // Start button
    wxButton* startBtn = new wxButton(rootPanel, ID_StartAudio, "Start Audio");
    mainSizer->Add(startBtn, 0, wxALIGN_CENTER | wxBOTTOM, 10);

    rootPanel->SetSizer(mainSizer);

    // ----- Event bindings -----
    startBtn->Bind(wxEVT_BUTTON, &MyFrame::OnStartAudio, this);
    m_deviceChoice->Bind(wxEVT_CHOICE, &MyFrame::OnDeviceChoice, this);

    Bind(wxEVT_MENU, &MyFrame::OnHello, this, ID_Hello);
    Bind(wxEVT_MENU, &MyFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_TIMER, &MyFrame::OnTimer, this, ID_TimerRefresh);

    // ----- Enumerate devices (output-only) -----
    InitOutputDevices();

    // ----- Start timer for live animation (≈30 FPS) -----
    m_timer.Start(33); // 33ms ~ 30 FPS
}

void MyFrame::InitOutputDevices()
{
    m_outputDeviceIndices.clear();
    m_deviceChoice->Clear();
    m_selectedDeviceIndex = paNoDevice;

    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        wxMessageBox("Failed to initialize PortAudio for device listing.",
                     "Error", wxOK | wxICON_ERROR);
        return;
    }

    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
    {
        wxMessageBox("No audio devices found.",
                     "Error", wxOK | wxICON_ERROR);
        Pa_Terminate();
        return;
    }

    int defaultOut = Pa_GetDefaultOutputDevice();
    int choiceIndexForDefault = -1;

    for (int i = 0; i < numDevices; ++i)
    {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;

        // Only show devices with at least 1 output channel
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

    // Select default output device if found
    if (choiceIndexForDefault >= 0)
    {
        m_deviceChoice->SetSelection(choiceIndexForDefault);
        m_selectedDeviceIndex = m_outputDeviceIndices[choiceIndexForDefault];
        SetOutputDeviceIndex(m_selectedDeviceIndex);
    }
    else if (!m_outputDeviceIndices.empty())
    {
        m_deviceChoice->SetSelection(0);
        m_selectedDeviceIndex = m_outputDeviceIndices[0];
        SetOutputDeviceIndex(m_selectedDeviceIndex);
    }

    Pa_Terminate();
}

void MyFrame::OnDeviceChoice(wxCommandEvent &)
{
    int sel = m_deviceChoice->GetSelection();
    if (sel == wxNOT_FOUND)
        return;

    if (sel >= 0 && sel < (int)m_outputDeviceIndices.size())
    {
        m_selectedDeviceIndex = m_outputDeviceIndices[sel];
        SetOutputDeviceIndex(m_selectedDeviceIndex);
        wxLogMessage("Selected output device index: %d", m_selectedDeviceIndex);
    }
}

void MyFrame::OnStartAudio(wxCommandEvent &)
{
    if (m_isPlaying)
    {
        wxLogMessage("Audio is already playing.");
        return;
    }

    if (m_selectedDeviceIndex == paNoDevice)
    {
        wxMessageBox("Please select an output device first.",
                     "No device selected",
                     wxOK | wxICON_WARNING);
        return;
    }

    m_isPlaying = true;
    SetStatusText("Audio running...");

    // Run start() (PortAudio) in background thread
    std::thread([this]() {
        int result = start(); // Uses gData + selected device via SetOutputDeviceIndex

        wxTheApp->CallAfter([this, result]() {
            SetStatusText(result == 0 ? "Audio finished." : "Audio error.");
            m_isPlaying = false;
        });
    }).detach();
}

void MyFrame::OnHello(wxCommandEvent &)
{
    wxLogMessage("Hello from wxWidgets!");
}

void MyFrame::OnAbout(wxCommandEvent &)
{
    wxMessageBox("wxWidgets + PortAudio live volume visualization demo.",
                 "About",
                 wxOK | wxICON_INFORMATION);
}

void MyFrame::OnExit(wxCommandEvent &)
{
    Close(true);
}

// Timer callback: refresh the panel so it reads latest volumes
void MyFrame::OnTimer(wxTimerEvent &)
{
    if (m_panel)
        m_panel->Refresh();
}


// ============================================================
//   Application entry point
// ============================================================

class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        // Initialize audio data (speakers, bounds, wavetable, etc.)
        initAudioData();

        MyFrame* frame = new MyFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);