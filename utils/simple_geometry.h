#pragma once

class Rect
{
public:
	Rect() : x(0), y(0), width(0), height(0) {}
	Rect(int xp, int yp, int w, int h) : x(xp), y(yp), width(w), height(h) {}

	int x, y, width, height;
};

class Dimension
{
public:
	Dimension() : width(0), height(0) {}
	Dimension(int w, int h) : width(w), height(h) {}
	
	int width, height;
};
