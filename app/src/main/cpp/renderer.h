#ifndef BLENDORA_RENDERER_H
#define BLENDORA_RENDERER_H

class Renderer {
public:
    bool init();
    void resize(int width, int height);
    void draw(float rotationX, float rotationY);
    void drawGrid(const float* mvp);
    void shutdown();
};

#endif
