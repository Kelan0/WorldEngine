#ifndef WORLDENGINE_UI_H
#define WORLDENGINE_UI_H


class UI {
public:
    UI();

    virtual ~UI() = 0;

    virtual void update(double dt) = 0;

    virtual void draw(double dt) = 0;
};


#endif //WORLDENGINE_UI_H
