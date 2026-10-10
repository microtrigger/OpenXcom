// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
// Asset-free production UI/extraction boundary fixture, not a game or display.
#include "../diagnostic-isolation/fixture_options.h"
#include "../../src/Interface/NumberText.h"
#include "../../src/Observer/PresentationObserver.h"
#include "../../src/Observer/PresentedCursorSurface.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <poll.h>
#include <sstream>
#include <unistd.h>

// Read only by the deliberately leaky temporary production-source mutant.
unsigned int pairedSourceValue = 0;
using namespace OpenXcom;
using namespace OpenXcom::Observer;
namespace {
const int Width = 80, Height = 40;
struct Composition {
    Surface surface;
    HudFrame frame;
    Composition() : surface(Width, Height) {}
    void compose(const std::string &scenario)
    {
        frame = HudFrame(); surface.clear();
        SDL_Color colors[256] = {};
        colors[1].r = 240; colors[1].g = 200; colors[1].b = 80;
        colors[2] = colors[1];
        surface.setPalette(colors);
        unsigned int value = 58;
        const std::size_t separator = scenario.find(':');
        const std::string mode = scenario.substr(0, separator);
        if (separator != std::string::npos) value = std::stoul(scenario.substr(separator + 1));
        for (int i = 0; i < 4; ++i)
        {
            NumberText widget(15, 5, 2 + i * 18, 2);
            widget.setPalette(colors); widget.setColor(0);
            widget.setValue(i == 0 ? value : 100);
            if (i == 0)
            {
                pairedSourceValue = widget.getValue();
                if (mode == "hidden") widget.setVisible(false);
                if (mode == "temporary") widget.setHidden(true);
                if (mode == "bordered") widget.setBordered(true);
                if (mode == "crop") { widget.getCrop()->w = 3; widget.getCrop()->h = 5; }
                if (mode == "offscreen") widget.setX(-20);
                if (mode == "clip") {
                    SDL_Rect clip = {2, 2, 3, 5};
                    SDL_SetClipRect(surface.getSurface(), &clip);
                }
            }
            widget.armPresentedDigits(&frame[i]);
            widget.blit(&surface); // Real draw(), digit sprites, transparency and candidate hook.
            widget.armPresentedDigits(0);
            SDL_SetClipRect(surface.getSurface(), 0);
        }
        if (mode == "overlaid" || mode == "covered")
        {
            surface.drawRect(2, 2, 15, 5, 0);
            if (mode == "overlaid") {
                NumberText overlay(15, 5, 2, 2);
                overlay.setPalette(colors); overlay.setColor(0); overlay.setValue(58);
                overlay.blit(&surface); // Opaque clearing plus real later glyph composition.
            }
        }
        if (mode == "alias") {
            surface.lock();
            for (int y = 0; y < Height; ++y) for (int x = 0; x < Width; ++x)
                if (surface.getPixel(x, y) == 1) surface.setPixel(x, y, 2);
            surface.unlock();
        }
        if (mode == "missing") frame[0] = HudDigits();
    }
    PixelCrop pixels() { return capturePresentedCursor(surface.getSurface(), Width, Height, 40, 20, true); }
    std::string hud() { return provePresentedHud(surface.getSurface(), Width, Height, frame, true); }
};
}
int main(int argc, char **argv)
{
    qualifyNormalDiagnostics();
    assert(argc == 2);
    const int descriptor = std::atoi(argv[1]);
    int monitor = dup(descriptor); assert(monitor >= 0);
    PresentationObserver producer(argv[1]);
    Composition composition;
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line == "quit") break;
        if (line.compare(0, 8, "compose ") == 0) {
            composition.compose(line.substr(8));
            std::cout << "{\"crop\":" << serializeCrop(composition.pixels())
                      << ",\"hud\":" << composition.hud() << "}" << std::endl;
            continue;
        }
        if (line == "wait-request") {
            pollfd p = {monitor, POLLIN, 0};
            assert(monitor >= 0 && poll(&p, 1, 2000) == 1 && (p.revents & POLLIN));
        }
        else if (line == "begin") producer.beginFrame();
        else if (line == "extract") producer.presented(composition.pixels(), composition.hud());
        else if (line == "reset") producer.resetPresentation();
        else if (line == "close") producer.close();
        else if (line != "status") return 2;
        if (!producer.enabled() && monitor >= 0) { ::close(monitor); monitor = -1; }
        std::cout << "{\"enabled\":" << (producer.enabled() ? "true" : "false")
                  << ",\"pending\":" << (producer.pending() ? "true" : "false") << "}" << std::endl;
    }
    if (monitor >= 0) ::close(monitor);
}
