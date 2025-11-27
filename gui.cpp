// gui.cpp
//
// wxWidgets GUI with:
// - Device selection (output-only)
// - Mode selection: Interactive (drag) vs Follow stdin (no dragging)
// - Room + speakers visualization
// - Live-updating volume sliders (short, wide right triangles with sweep fill)
// - Background audio thread
// - wxTimer to animate the GUI
//

#include "start.h"                 // int start(); void initAudioData();
#include "portaudio_listener.h"    // paTestData, CHANNEL_COUNT, etc.

#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstring>
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
    SpeakerPanel(wxWindow* parent, paTestData* data, bool interactiveMode)
        : wxPanel(parent)
        , m_data(data)
        , m_interactive(interactiveMode)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &SpeakerPanel::OnPaint, this);

        // Always bind mouse events; we gate behaviour with m_interactive
        Bind(wxEVT_LEFT_DOWN,    &SpeakerPanel::OnLeftDown,   this);
        Bind(wxEVT_LEFT_UP,      &SpeakerPanel::OnLeftUp,     this);
        Bind(wxEVT_MOTION,       &SpeakerPanel::OnMouseMove,  this);
        Bind(wxEVT_LEAVE_WINDOW, &SpeakerPanel::OnMouseLeave, this);
    }

    void SetInteractive(bool interactive)
    {
        m_interactive = interactive;
        // Clear any drag state when we switch modes
        m_mouseDown        = false;
        m_draggingListener = false;
        m_dragSpeakerIndex = -1;
        m_selectedSpeaker  = -1;
        if (HasCapture())
            ReleaseMouse();
        Refresh();
    }

private:
    paTestData* m_data = nullptr;

    bool m_interactive       = true;
    bool m_mouseDown         = false;
    bool m_draggingListener  = false;
    int  m_dragSpeakerIndex  = -1;   // which speaker we're dragging
    int  m_selectedSpeaker   = -1;   // which speaker to show distances from

    // Compute scale and bounds for current subject, keeping a margin.
    float computeScaleForCurrentBounds(int w, int h,
                                       float& minX, float& minY,
                                       float& maxX, float& maxY) const
    {
        if (!m_data)
        {
            minX = minY = -1.0f;
            maxX = maxY =  1.0f;
            return 1.0f;
        }

        minX = m_data->subjectBounds[0].x;
        minY = m_data->subjectBounds[0].y;
        maxX = m_data->subjectBounds[1].x;
        maxY = m_data->subjectBounds[1].y;

        const int margin = 20;

        float maxAbsX = std::max(std::fabs(minX), std::fabs(maxX));
        float maxAbsY = std::max(std::fabs(minY), std::fabs(maxY));

        if (maxAbsX < 0.001f) maxAbsX = 1.0f;
        if (maxAbsY < 0.001f) maxAbsY = 1.0f;

        float scaleX = (w / 2.0f - margin) / maxAbsX;
        float scaleY = (h / 2.0f - margin) / maxAbsY;
        float scale  = (scaleX < scaleY ? scaleX : scaleY);
        if (scale <= 0.0f) scale = 1.0f;

        return scale;
    }

    // Fixed world->screen mapping: (0,0) is center of panel
    wxPoint worldToScreen(float x, float y, int w, int h, float scale) const
    {
        float cx = w * 0.5f;
        float cy = h * 0.5f;

        float sx = cx + x * scale;
        float sy = cy - y * scale; // invert Y so +Y is up

        return wxPoint((int)sx, (int)sy);
    }

    Point screenToWorld(int px, int py, int w, int h, float scale) const
    {
        float cx = w * 0.5f;
        float cy = h * 0.5f;

        float x = (px - cx) / scale;
        float y = (cy - py) / scale; // invert Y back

        return Point{ x, y };
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
        float minX, minY, maxX, maxY;
        float scale = computeScaleForCurrentBounds(w, h, minX, minY, maxX, maxY);

        // ----- Draw room rectangle using worldToScreen -----
        dc.SetPen(*wxBLACK_PEN);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        wxPoint topLeft     = worldToScreen(minX, maxY, w, h, scale);
        wxPoint topRight    = worldToScreen(maxX, maxY, w, h, scale);
        wxPoint bottomLeft  = worldToScreen(minX, minY, w, h, scale);

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
            float vol = m_data->channelGains[i];

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

            // Filled region for current volume
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

        float dirLength = 1.0f; // metres

        float endX = L.x + dirLength * std::sin(yawRadians); // X points right
        float endY = L.y + dirLength * std::cos(yawRadians); // Y points forward

        wxPoint endPoint = worldToScreen(endX, endY, w, h, scale);

        dc.SetPen(wxPen(*wxRED, 2)); // red line for facing direction
        dc.DrawLine(lp, endPoint);

        float coneAngle  = 15.0f * (M_PI / 180.0f); // 15° half-angle
        float coneLength = dirLength * 0.8f;        // slightly shorter

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

        // ----- Distance overlay from selected speaker -----
        if (m_interactive &&
            m_mouseDown &&
            m_selectedSpeaker >= 0 &&
            m_selectedSpeaker < CHANNEL_COUNT)
        {
            Point spSel = m_data->speakerPositions[m_selectedSpeaker];
            wxPoint pSel = worldToScreen(spSel.x, spSel.y, w, h, scale);

            dc.SetPen(wxPen(*wxBLACK, 1, wxPENSTYLE_DOT));

            for (int i = 0; i < CHANNEL_COUNT; ++i)
            {
                if (i == m_selectedSpeaker)
                    continue;

                Point sp = m_data->speakerPositions[i];
                wxPoint p = worldToScreen(sp.x, sp.y, w, h, scale);

                // Line between speakers
                dc.DrawLine(pSel, p);

                // Distance label (world units -> metres)
                float dx = sp.x - spSel.x;
                float dy = sp.y - spSel.y;
                float dist = std::sqrt(dx*dx + dy*dy);

                wxString lbl;
                lbl.Printf("%.2f m", dist);

                int midX = (pSel.x + p.x) / 2;
                int midY = (pSel.y + p.y) / 2;
                dc.DrawText(lbl, midX + 3, midY + 3);
            }
        }
    }

    void OnLeftDown(wxMouseEvent& evt)
    {
        if (!m_interactive || !m_data)
            return;

        int w, h;
        GetClientSize(&w, &h);

        float minX, minY, maxX, maxY;
        float scale = computeScaleForCurrentBounds(w, h, minX, minY, maxX, maxY);

        wxPoint mousePos = evt.GetPosition();

        // ----- Hit-test listener -----
        Point L = m_data->currentListenerPosition;
        wxPoint lp = worldToScreen(L.x, L.y, w, h, scale);

        int dx = mousePos.x - lp.x;
        int dy = mousePos.y - lp.y;
        int dist2 = dx*dx + dy*dy;
        const int listenerRadiusPx = 10;

        if (dist2 <= listenerRadiusPx * listenerRadiusPx)
        {
            m_mouseDown        = true;
            m_draggingListener = true;
            m_dragSpeakerIndex = -1;
            m_selectedSpeaker  = -1;
            CaptureMouse();
            Refresh();
            return;
        }

        // ----- Hit-test speakers -----
        const int speakerBoxSize = 12;
        int halfBox = speakerBoxSize / 2;

        for (int i = 0; i < CHANNEL_COUNT; ++i)
        {
            Point sp = m_data->speakerPositions[i];
            wxPoint p = worldToScreen(sp.x, sp.y, w, h, scale);

            wxRect rect(p.x - halfBox, p.y - halfBox,
                        speakerBoxSize, speakerBoxSize);

            if (rect.Contains(mousePos))
            {
                m_mouseDown        = true;
                m_draggingListener = false;
                m_dragSpeakerIndex = i;
                m_selectedSpeaker  = i; // for distance overlay
                CaptureMouse();
                Refresh();
                return;
            }
        }
    }

    void OnMouseMove(wxMouseEvent& evt)
    {
        if (!m_interactive || !m_mouseDown || !m_data)
            return;

        int w, h;
        GetClientSize(&w, &h);

        float minX, minY, maxX, maxY;
        float scale = computeScaleForCurrentBounds(w, h, minX, minY, maxX, maxY);

        wxPoint mousePos = evt.GetPosition();
        Point world = screenToWorld(mousePos.x, mousePos.y, w, h, scale);

        if (m_draggingListener)
        {
            m_data->currentListenerPosition = world;
        }
        else if (m_dragSpeakerIndex >= 0 &&
                 m_dragSpeakerIndex < CHANNEL_COUNT)
        {
            m_data->speakerPositions[m_dragSpeakerIndex] = world;
        }

        Refresh();
    }

    void OnLeftUp(wxMouseEvent& evt)
    {
        if (!m_interactive)
            return;

        m_mouseDown        = false;
        m_draggingListener = false;
        m_dragSpeakerIndex = -1;
        m_selectedSpeaker  = -1;

        if (HasCapture())
            ReleaseMouse();

        Refresh();
    }

    void OnMouseLeave(wxMouseEvent& evt)
    {
        if (!m_interactive)
            return;

        if (m_mouseDown && HasCapture())
            ReleaseMouse();

        m_mouseDown        = false;
        m_draggingListener = false;
        m_dragSpeakerIndex = -1;
        m_selectedSpeaker  = -1;

        Refresh();
    }
};

// ============================================================
//   Main window (MyFrame) with device selection + timer + mode
// ============================================================

enum
{
    ID_Hello = 1,
    ID_TimerRefresh,
    ID_DeviceChoice,
    ID_StartAudio,
    ID_ModeChoice
};

class MyFrame : public wxFrame
{
public:
    MyFrame(bool interactiveMode);

private:
    bool m_interactiveMode = true;

private:
    void OnStartAudio(wxCommandEvent &event);
    void OnExit(wxCommandEvent &event);
    void OnAbout(wxCommandEvent &event);
    void OnHello(wxCommandEvent &event);
    void OnTimer(wxTimerEvent &event);
    void OnDeviceChoice(wxCommandEvent &event);
    void OnModeChoice(wxCommandEvent &event);

    wxTimer       m_timer;
    wxChoice*     m_deviceChoice = nullptr;
    wxChoice*     m_modeChoice   = nullptr;
    SpeakerPanel* m_panel        = nullptr;

    // Keep track of which PaDeviceIndex each wxChoice entry corresponds to
    std::vector<int> m_outputDeviceIndices;
};

MyFrame::MyFrame(bool interactiveMode)
    : wxFrame(nullptr, wxID_ANY, "PortAudio + wxWidgets (Live Volumes)",
              wxDefaultPosition, wxSize(980, 600)),
      m_interactiveMode(interactiveMode),
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
    if (m_interactiveMode)
        SetStatusText("Interactive mode: drag listener & speakers.");
    else
        SetStatusText("Stdin mode: positions controlled via stdin.");

    // ----- Layout: device row + panel + button -----
    wxPanel* rootPanel  = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Device + mode selection row
    wxBoxSizer* deviceSizer = new wxBoxSizer(wxHORIZONTAL);

    deviceSizer->Add(new wxStaticText(rootPanel, wxID_ANY, "Output device:"),
                     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_deviceChoice = new wxChoice(rootPanel, ID_DeviceChoice);
    deviceSizer->Add(m_deviceChoice, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 15);

    deviceSizer->Add(new wxStaticText(rootPanel, wxID_ANY, "Mode:"),
                     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_modeChoice = new wxChoice(rootPanel, ID_ModeChoice);
    m_modeChoice->Append("Interactive (drag)");
    m_modeChoice->Append("Follow stdin (no dragging)");
    m_modeChoice->SetSelection(m_interactiveMode ? 0 : 1);
    deviceSizer->Add(m_modeChoice, 0, wxALIGN_CENTER_VERTICAL);

    mainSizer->Add(deviceSizer, 0, wxEXPAND | wxALL, 10);

    // Visualization panel
    m_panel = new SpeakerPanel(rootPanel, &gData, m_interactiveMode);
    mainSizer->Add(m_panel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    // Start button
    wxButton* startBtn = new wxButton(rootPanel, ID_StartAudio, "Start Audio");
    mainSizer->Add(startBtn, 0, wxALIGN_CENTER | wxBOTTOM, 10);

    rootPanel->SetSizer(mainSizer);

    // ----- Timer for periodic refresh -----
    m_timer.Start(50); // ~20 FPS

    // Events
    Bind(wxEVT_TIMER,        &MyFrame::OnTimer,        this, ID_TimerRefresh);
    Bind(wxEVT_MENU,         &MyFrame::OnExit,         this, wxID_EXIT);
    Bind(wxEVT_MENU,         &MyFrame::OnAbout,        this, wxID_ABOUT);
    Bind(wxEVT_MENU,         &MyFrame::OnHello,        this, ID_Hello);
    Bind(wxEVT_CHOICE,       &MyFrame::OnDeviceChoice, this, ID_DeviceChoice);
    Bind(wxEVT_CHOICE,       &MyFrame::OnModeChoice,   this, ID_ModeChoice);
    Bind(wxEVT_BUTTON,       &MyFrame::OnStartAudio,   this, ID_StartAudio);

    // Enumerate audio devices
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

    if (!m_outputDeviceIndices.empty())
    {
        int toSelect = (choiceIndexForDefault >= 0)
                       ? choiceIndexForDefault
                       : 0;
        m_deviceChoice->SetSelection(toSelect);
        SetOutputDeviceIndex(m_outputDeviceIndices[toSelect]);
    }

    // Ensure panel respects initial mode
    if (m_panel)
        m_panel->SetInteractive(m_interactiveMode);
}

void MyFrame::OnStartAudio(wxCommandEvent &event)
{
    // Start the audio processing in a background thread
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
    bool interactive = (sel == 0);

    m_interactiveMode = interactive;

    if (m_panel)
        m_panel->SetInteractive(m_interactiveMode);

    if (m_interactiveMode)
        SetStatusText("Interactive mode: drag listener & speakers.");
    else
        SetStatusText("Stdin mode: positions controlled via stdin.");
}

// ============================================================
//   wxWidgets application
// ============================================================

class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        // Initialize audio data (speakers, bounds, wavetable, etc.)
        initAudioData();

        bool interactiveMode = true;

        // Still honour --stdin-mode if you want a default
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "--stdin-mode") == 0)
            {
                interactiveMode = false;
            }
        }

        MyFrame* frame = new MyFrame(interactiveMode);
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);