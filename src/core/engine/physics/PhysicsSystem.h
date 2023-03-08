#ifndef WORLDENGINE_PHYSICSSYSTEM_H
#define WORLDENGINE_PHYSICSSYSTEM_H

#include "core/core.h"
#include "core/engine/physics/RigidBody.h"
#include "core/engine/scene/Scene.h"

class Scene;

class PhysicsSystem {
public:
    PhysicsSystem();

    ~PhysicsSystem();

    bool init();

    void preTick(double dt);

    void tick(double dt);

    void preRender(double dt);

    void setScene(Scene* scene);

    Scene* getScene() const;

private:
    void onRigidBodyAdded(ComponentAddedEvent<RigidBody>* event);

    void onRigidBodyRemoved(ComponentRemovedEvent<RigidBody>* event);
private:
    Scene* m_scene;
};


#endif //WORLDENGINE_PHYSICSSYSTEM_H
