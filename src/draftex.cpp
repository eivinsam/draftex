

#include <oui_window.h>
#include <oui_text.h>

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")


int main()
{
	oui::Window window{ { "draftex", 1280, 720 } };

	oui::Font font("Segoe UI", int(24*window.dpiFactor()));

	while (window.update())
	{
		window.clear(oui::colors::white);
		auto area = window.area();
		
		font.drawText(area, "hello", oui::colors::black);

		oui::fill(area.popBottom(40), oui::colors::blue);
	}


	return 0;
}
