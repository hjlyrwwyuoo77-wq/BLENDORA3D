#ifndef BLENDORA_RENDERER_H
#define BLENDORA_RENDERER_H

class Renderer {
public:
    bool init();
    void resize(int width, int height);
    void draw(float rotationX, float rotationY,
              float moveX, float moveY, float objectScale);
    void drawGrid(const float* mvp);
    void shutdown();
};

#endif
