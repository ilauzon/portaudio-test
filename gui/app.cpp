#include <wx/wx.h>
#include <cstring>
#include <cstdlib> // Required for setenv
#include "main_frame.h"
#include "../start.h"

class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        setenv("GTK_THEME", "Adwaita:dark", 1);

        initAudioData();

        bool interactiveMode = true;

        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "--stdin-mode") == 0)
            {
                interactiveMode = false;
            }
        }

        MyFrame* frame = new MyFrame(interactiveMode);

        frame->SetBackgroundColour(wxColour(30, 30, 30));
        
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);