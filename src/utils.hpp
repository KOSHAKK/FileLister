#include "imgui.h"
#include <Windows.h>

using namespace Gdiplus;

void DrawCrosshairGDIPlus(HDC hdc, int x, int y, int size, int length, Color color)
{
    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    Pen pen(color, size);

    graphics.DrawLine(&pen, x - length, y, x + length, y);
    graphics.DrawLine(&pen, x, y - length, x, y + length);
}