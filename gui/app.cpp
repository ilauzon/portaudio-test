#include <wx/wx.h>
#include "main_frame.h"
#include "../start.h"

class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
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
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);